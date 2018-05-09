#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" Usage:
    make_intl_data.py langtags [language-subtag-registry.txt]
    make_intl_data.py tzdata
    make_intl_data.py currency

    Target "langtags":
    This script extracts information about mappings between deprecated and
    current BCP 47 language tags from the IANA Language Subtag Registry and
    converts it to JavaScript object definitions in
    LangTagMappingsGenerated.js. The definitions are used in Intl.js.

    The IANA Language Subtag Registry is imported from
    https://www.iana.org/assignments/language-subtag-registry
    and uses the syntax specified in
    https://tools.ietf.org/html/rfc5646#section-3


    Target "tzdata":
    This script computes which time zone informations are not up-to-date in ICU
    and provides the necessary mappings to workaround this problem.
    https://ssl.icu-project.org/trac/ticket/12044


    Target "currency":
    Generates the mapping from currency codes to decimal digits used for them.
"""

from __future__ import print_function
import os
import re
import io
import sys
import tarfile
import tempfile
from contextlib import closing
from functools import partial, total_ordering
from itertools import chain, groupby, tee
from operator import attrgetter, itemgetter

if sys.version_info.major == 2:
    from itertools import ifilter as filter, ifilterfalse as filterfalse, imap as map
    from urllib2 import urlopen, Request as UrlRequest
    from urlparse import urlsplit
else:
    from itertools import filterfalse
    from urllib.request import urlopen, Request as UrlRequest
    from urllib.parse import urlsplit

def readRegistryRecord(registry):
    """ Yields the records of the IANA Language Subtag Registry as dictionaries. """
    record = {}
    for line in registry:
        line = line.strip()
        if line == "":
            continue
        if line == "%%":
            yield record
            record = {}
        else:
            if ":" in line:
                key, value = line.split(":", 1)
                key, value = key.strip(), value.strip()
                record[key] = value
            else:
                # continuation line
                record[key] += " " + line
    if record:
        yield record
    return

def readRegistry(registry):
    """ Reads IANA Language Subtag Registry and extracts information for Intl.js.

        Information extracted:
        - grandfatheredMappings: mappings from grandfathered tags to preferred
          complete language tags
        - redundantMappings: mappings from redundant tags to preferred complete
          language tags
        - languageMappings: mappings from language subtags to preferred subtags
        - regionMappings: mappings from region subtags to preferred subtags
        - variantMappings: mappings from complete language tags to preferred
          complete language tags
        - extlangMappings: mappings from extlang subtags to preferred subtags,
          with prefix to be removed
        Returns these six mappings as dictionaries, along with the registry's
        file date.

        We also check that extlang mappings don't generate preferred values
        which in turn are subject to language subtag mappings, so that
        CanonicalizeLanguageTag can process subtags sequentially.
    """
    grandfatheredMappings = {}
    redundantMappings = {}
    languageMappings = {}
    regionMappings = {}
    variantMappings = {}
    extlangMappings = {}
    extlangSubtags = []

    for record in readRegistryRecord(registry):
        if "File-Date" in record:
            fileDate = record["File-Date"]
            continue

        if record["Type"] == "grandfathered":
            # Grandfathered tags don't use standard syntax, so
            # CanonicalizeLanguageTag expects the mapping table to provide
            # the final form for all.
            # For grandfatheredMappings, keys must be in lower case; values in
            # the case used in the registry.
            tag = record["Tag"]
            if "Preferred-Value" in record:
                grandfatheredMappings[tag.lower()] = record["Preferred-Value"]
            else:
                grandfatheredMappings[tag.lower()] = tag
        elif record["Type"] == "redundant":
            # For redundantMappings, keys and values must be in the case used
            # in the registry.
            if "Preferred-Value" in record:
                redundantMappings[record["Tag"]] = record["Preferred-Value"]
        elif record["Type"] == "language":
            # For languageMappings, keys and values must be in the case used
            # in the registry.
            subtag = record["Subtag"]
            if "Preferred-Value" in record:
                # The 'Prefix' field is not allowed for language records.
                # https://tools.ietf.org/html/rfc5646#section-3.1.2
                assert "Prefix" not in record, "language subtags can't have a prefix"
                languageMappings[subtag] = record["Preferred-Value"]
        elif record["Type"] == "region":
            # For regionMappings, keys and values must be in the case used in
            # the registry.
            subtag = record["Subtag"]
            if "Preferred-Value" in record:
                # The 'Prefix' field is not allowed for region records.
                # https://tools.ietf.org/html/rfc5646#section-3.1.2
                assert "Prefix" not in record, "region subtags can't have a prefix"
                regionMappings[subtag] = record["Preferred-Value"]
        elif record["Type"] == "script":
            if "Preferred-Value" in record:
                # The registry currently doesn't contain mappings for scripts.
                raise Exception("Unexpected mapping for script subtags")
        elif record["Type"] == "variant":
            # For variantMappings, keys and values must be in the case used in
            # the registry.
            if "Preferred-Value" in record:
                if "Prefix" not in record:
                    raise Exception("Unexpected mapping for variant subtags")
                tag = "{}-{}".format(record["Prefix"], record["Subtag"])
                variantMappings[tag] = record["Preferred-Value"]
        elif record["Type"] == "extlang":
            # For extlangMappings, keys must be in the case used in the
            # registry; values are records with the preferred value and the
            # prefix to be removed.
            subtag = record["Subtag"]
            extlangSubtags.append(subtag)
            if "Preferred-Value" in record:
                preferred = record["Preferred-Value"]
                # The 'Preferred-Value' and 'Subtag' fields MUST be identical.
                # https://tools.ietf.org/html/rfc5646#section-2.2.2
                assert preferred == subtag, "{0} = {1}".format(preferred, subtag)
                prefix = record["Prefix"]
                extlangMappings[subtag] = {"preferred": preferred, "prefix": prefix}
        else:
            # No other types are allowed by
            # https://tools.ietf.org/html/rfc5646#section-3.1.3
            assert False, "Unrecognized Type: {0}".format(record["Type"])

    # Check that mappings for language subtags and extlang subtags don't affect
    # each other.
    for extlang in extlangSubtags:
        if extlang in languageMappings:
            raise Exception("Conflict: extlang with lang mapping: " + extlang)

    # Special case for heploc.
    assert variantMappings["ja-Latn-hepburn-heploc"] == "alalc97"
    variantMappings["ja-Latn-hepburn-heploc"] = "ja-Latn-alalc97"

    # ValidateAndCanonicalizeLanguageTag in CommonFunctions.js expects
    # redundantMappings contains no 2*3ALPHA.
    assert all(len(lang) > 3 for lang in redundantMappings.keys())

    return {"fileDate": fileDate,
            "grandfatheredMappings": grandfatheredMappings,
            "redundantMappings": redundantMappings,
            "languageMappings": languageMappings,
            "regionMappings": regionMappings,
            "variantMappings": variantMappings,
            "extlangMappings": extlangMappings}

def writeMappingHeader(println, description, fileDate, url):
    if type(description) is not list:
        description = [description]
    for desc in description:
        println(u"// {0}".format(desc))
    println(u"// Derived from IANA Language Subtag Registry, file date {0}.".format(fileDate))
    println(u"// {0}".format(url))

def writeMappingsVar(println, mapping, name, description, fileDate, url):
    """ Writes a variable definition with a mapping table.

        Writes the contents of dictionary |mapping| through the |println|
        function with the given variable name and a comment with description,
        fileDate, and URL.
    """
    println(u"")
    writeMappingHeader(println, description, fileDate, url)
    println(u"var {0} = {{".format(name))
    for key in sorted(mapping):
        if not isinstance(mapping[key], dict):
            value = '"{0}"'.format(mapping[key])
        else:
            preferred = mapping[key]["preferred"]
            prefix = mapping[key]["prefix"]
            if key != preferred:
                raise Exception("Expected '{0}' matches preferred locale '{1}'".format(key, preferred))
            value = '"{0}"'.format(prefix)
        println(u'    "{0}": {1},'.format(key, value))
    println(u"};")

def writeMappingsFunction(println, variantMappings, redundantMappings, extlangMappings, description, fileDate, url):
    """ Writes a function definition which performs language tag mapping.

        Processes the contents of dictionaries |variantMappings| and
        |redundantMappings| through the |println| function with the given
        function name and a comment with description, fileDate, and URL.
    """

    class Subtag(object):
        Language, ExtLang, Script, Region, Variant = range(5)
        Invalid = -1

    def splitSubtags(tag):
        seenLanguage = False
        for subtag in tag.split("-"):
            # language = 2*3ALPHA / 4ALPHA / 5*8ALPHA
            if len(subtag) in range(2, 8+1) and subtag.isalpha() and not seenLanguage:
                seenLanguage = True
                kind = Subtag.Language

            # extlang = 3ALPHA
            elif len(subtag) == 3 and subtag.isalpha() and seenLanguage:
                kind = Subtag.ExtLang

            # script = 4ALPHA
            elif len(subtag) == 4 and subtag.isalpha():
                kind = Subtag.Script

            # region = 2ALPHA / 3DIGIT
            elif ((len(subtag) == 2 and subtag.isalpha()) or
                  (len(subtag) == 3 and subtag.isdigit())):
                kind = Subtag.Region

            # variant = 5*8alphanum / (DIGIT 3alphanum)
            elif ((len(subtag) in range(5, 8+1) and subtag.isalnum()) or
                  (len(subtag) == 4 and subtag[0].isdigit() and subtag[1:].isalnum())):
                kind = Subtag.Variant

            else:
                assert False, "unexpected language tag '{}'".format(key)

            yield (kind, subtag)

    def language(tag):
        (kind, subtag) = next(splitSubtags(tag))
        assert kind == Subtag.Language
        return subtag

    def variants(tag):
        return [v for (k, v) in splitSubtags(tag) if k == Subtag.Variant]

    def emitCompare(tag, preferred, isFirstLanguageTag):
        def println_indent(level, *args):
            println(u" " * (4 * level - 1), *args)
        println2 = partial(println_indent, 2)
        println3 = partial(println_indent, 3)

        def maybeNext(it):
            dummy = (Subtag.Invalid, "")
            return next(it, dummy)

        # Add a comment for the language tag mapping.
        println2(u"// {} -> {}".format(tag, preferred))

        # Compare the input language tag with the current language tag.
        cond = []
        extlangIndex = 1
        lastVariant = None
        for (kind, subtag) in splitSubtags(tag):
            if kind == Subtag.Language:
                continue

            if kind == Subtag.ExtLang:
                assert extlangIndex in [1, 2, 3],\
                       "Language-Tag permits no more than three extlang subtags"
                cond.append('tag.extlang{} === "{}"'.format(extlangIndex, subtag))
                extlangIndex += 1
            elif kind == Subtag.Script:
                cond.append('tag.script === "{}"'.format(subtag))
            elif kind == Subtag.Region:
                cond.append('tag.region === "{}"'.format(subtag))
            else:
                assert kind == Subtag.Variant
                if lastVariant is None:
                    cond.append("tag.variants.length >= {}".format(len(variants(tag))))
                    cond.append('callFunction(ArrayIndexOf, tag.variants, "{}") > -1'.format(subtag))
                else:
                    cond.append('callFunction(ArrayIndexOf, tag.variants, "{}", callFunction(ArrayIndexOf, tag.variants, "{}") + 1) > -1'.format(subtag, lastVariant))
                lastVariant = subtag

        # Require exact matches for redundant language tags.
        if tag in redundantMappings:
            tag_it = splitSubtags(tag)
            tag_next = partial(maybeNext, tag_it)
            (tag_kind, _) = tag_next()

            assert tag_kind == Subtag.Language
            (tag_kind, _) = tag_next()

            subtags = ([(Subtag.ExtLang, "extlang{}".format(i)) for i in range(1, 3+1)] +
                       [(Subtag.Script, "script"), (Subtag.Region, "region")])
            for kind, prop_name in subtags:
                if tag_kind == kind:
                    (tag_kind, _) = tag_next()
                else:
                    cond.append("tag.{} === undefined".format(prop_name))

            cond.append("tag.variants.length === {}".format(len(variants(tag))))
            while tag_kind == Subtag.Variant:
                (tag_kind, _) = tag_next()

            cond.append("tag.extensions.length === 0")
            cond.append("tag.privateuse === undefined")
            assert list(tag_it) == [], "unhandled tag subtags"

        # Emit either:
        #
        #   if (cond) {
        #
        # or:
        #
        #   if (cond_1 &&
        #       cond_2 &&
        #       ...
        #       cond_n)
        #   {
        #
        # depending on the number of conditions.
        ifOrElseIf = "if" if isFirstLanguageTag else "else if"
        assert len(cond) > 0, "expect at least one subtag condition"
        if len(cond) == 1:
            println2(u"{} ({}) {{".format(ifOrElseIf, cond[0]))
        else:
            println2(u"{} ({} &&".format(ifOrElseIf, cond[0]))
            for c in cond[1:-1]:
                println2(u"{}{} &&".format(" " * (len(ifOrElseIf) + 2), c))
            println2(u"{}{})".format(" " * (len(ifOrElseIf) + 2), cond[-1]))
            println2(u"{")

        # Iterate over all subtags of |tag| and |preferred| and update |tag|
        # with |preferred| in the process. |tag| is modified in-place to use
        # the preferred values.
        tag_it = splitSubtags(tag)
        tag_next = partial(maybeNext, tag_it)
        (tag_kind, tag_subtag) = tag_next()

        preferred_it = splitSubtags(preferred)
        preferred_next = partial(maybeNext, preferred_it)
        (preferred_kind, preferred_subtag) = preferred_next()

        # Update the language subtag.
        assert tag_kind == Subtag.Language and preferred_kind == Subtag.Language
        if tag_subtag != preferred_subtag:
            println3(u'tag.language = "{}";'.format(preferred_subtag))
        (tag_kind, tag_subtag) = tag_next()
        (preferred_kind, preferred_subtag) = preferred_next()

        # Remove any extlang subtags per RFC 5646, 4.5:
        # 'The canonical form contains no 'extlang' subtags.'
        # https://tools.ietf.org/html/rfc5646#section-4.5
        assert preferred_kind != Subtag.ExtLang
        extlangIndex = 1
        while tag_kind == Subtag.ExtLang:
            assert extlangIndex in [1, 2, 3],\
                   "Language-Tag permits no more than three extlang subtags"
            println3(u"tag.extlang{} = undefined;".format(extlangIndex))
            extlangIndex += 1
            (tag_kind, tag_subtag) = tag_next()

        # Update the script and region subtags.
        for kind, prop_name in [(Subtag.Script, "script"), (Subtag.Region, "region")]:
            if tag_kind == kind and preferred_kind == kind:
                if tag_subtag != preferred_subtag:
                    println3(u'tag.{} = "{}";'.format(prop_name, preferred_subtag))
                (tag_kind, tag_subtag) = tag_next()
                (preferred_kind, preferred_subtag) = preferred_next()
            elif tag_kind == kind:
                println3(u"tag.{} = undefined;".format(prop_name))
                (tag_kind, tag_subtag) = tag_next()
            elif preferred_kind == kind:
                println3(u'tag.{} = "{}";'.format(prop_name, preferred_subtag))
                (preferred_kind, preferred_subtag) = preferred_next()

        # Update variant subtags.
        if tag_kind == Subtag.Variant or preferred_kind == Subtag.Variant:
            # JS doesn't provide an easy way to remove elements from an array
            # which doesn't trigger Symbol.species, so we need to create a new
            # array and copy all elements.
            println3(u"var newVariants = [];")

            # Copy all variant subtags, ignoring those which should be removed.
            println3(u"for (var i = 0; i < tag.variants.length; i++) {")
            println3(u"    var variant = tag.variants[i];")
            while tag_kind == Subtag.Variant:
                println3(u'    if (variant === "{}")'.format(tag_subtag))
                println3(u"        continue;")
                (tag_kind, tag_subtag) = tag_next()
            println3(u"    _DefineDataProperty(newVariants, newVariants.length, variant);")
            println3(u"}")

            # Add the new variants, unless already present.
            while preferred_kind == Subtag.Variant:
                println3(u'if (callFunction(ArrayIndexOf, newVariants, "{}") < 0)'.format(preferred_subtag))
                println3(u'    _DefineDataProperty(newVariants, newVariants.length, "{}");'.format(preferred_subtag))
                (preferred_kind, preferred_subtag) = preferred_next()

            # Update the property.
            println3(u"tag.variants = newVariants;")

        # Ensure both language tags were completely processed.
        assert list(tag_it) == [], "unhandled tag subtags"
        assert list(preferred_it) == [], "unhandled preferred subtags"

        println2(u"}")

    # Remove mappings for redundant language tags which are from our point of
    # view, wait for it, redundant, because there is an equivalent extlang
    # mapping.
    #
    # For example this entry for the redundant tag "zh-cmn":
    #
    #   Type: redundant
    #   Tag: zh-cmn
    #   Preferred-Value: cmn
    #
    # Can also be expressed through the extlang mapping for "cmn":
    #
    #   Type: extlang
    #   Subtag: cmn
    #   Preferred-Value: cmn
    #   Prefix: zh
    #
    def hasExtlangMapping(tag, preferred):
        tag_it = splitSubtags(tag)
        (_, tag_lang) = next(tag_it)
        (tag_kind, tag_extlang) = next(tag_it)

        preferred_it = splitSubtags(preferred)
        (_, preferred_lang) = next(preferred_it)

        # Return true if the mapping is for an extlang language and the extlang
        # mapping table contains an equivalent entry and any trailing elements,
        # if present, are the same.
        return (tag_kind == Subtag.ExtLang and
                (tag_extlang, {"preferred": preferred_lang, "prefix": tag_lang}) in extlangMappings.items() and
                list(tag_it) == list(preferred_it))

    # Create a single mapping for variant and redundant tags, ignoring the
    # entries which are also covered through extlang mappings.
    langTagMappings = {tag: preferred
                       for mapping in [variantMappings, redundantMappings]
                       for (tag, preferred) in mapping.items()
                       if not hasExtlangMapping(tag, preferred)}

    println(u"")
    println(u"/* eslint-disable complexity */")
    writeMappingHeader(println, description, fileDate, url)
    println(u"function updateLangTagMappings(tag) {")
    println(u'    assert(IsObject(tag), "tag is an object");')
    println(u'    assert(!hasOwn("grandfathered", tag), "tag is not a grandfathered tag");')
    println(u"")

    # Switch on the language subtag.
    println(u"    switch (tag.language) {")
    for lang in sorted({language(tag) for tag in langTagMappings}):
        println(u'      case "{}":'.format(lang))
        isFirstLanguageTag = True
        for tag in sorted(tag for tag in langTagMappings if language(tag) == lang):
            assert not isinstance(langTagMappings[tag], dict),\
                   "only supports complete language tags"
            emitCompare(tag, langTagMappings[tag], isFirstLanguageTag)
            isFirstLanguageTag = False
        println(u"        break;")
    println(u"    }")

    println(u"}")
    println(u"/* eslint-enable complexity */")

def writeLanguageTagData(println, data, url):
    """ Writes the language tag data to the Intl data file. """

    fileDate = data["fileDate"]
    grandfatheredMappings = data["grandfatheredMappings"]
    redundantMappings = data["redundantMappings"]
    languageMappings = data["languageMappings"]
    regionMappings = data["regionMappings"]
    variantMappings = data["variantMappings"]
    extlangMappings = data["extlangMappings"]

    writeMappingsFunction(println, variantMappings, redundantMappings, extlangMappings,
                          "Mappings from complete tags to preferred values.", fileDate, url)
    writeMappingsVar(println, grandfatheredMappings, "grandfatheredMappings",
                     "Mappings from grandfathered tags to preferred values.", fileDate, url)
    writeMappingsVar(println, languageMappings, "languageMappings",
                     "Mappings from language subtags to preferred values.", fileDate, url)
    writeMappingsVar(println, regionMappings, "regionMappings",
                     "Mappings from region subtags to preferred values.", fileDate, url)
    writeMappingsVar(println, extlangMappings, "extlangMappings",
                     ["Mappings from extlang subtags to preferred values.",
                      "All current deprecated extlang subtags have the form `<prefix>-<extlang>`",
                      "and their preferred value is exactly equal to `<extlang>`. So each key in",
                      "extlangMappings acts both as the extlang subtag and its preferred value."],
                     fileDate, url)

def updateLangTags(args):
    """ Update the LangTagMappingsGenerated.js file. """
    url = args.url
    out = args.out
    filename = args.file

    print("Arguments:")
    print("\tDownload url: %s" % url)
    print("\tLocal registry: %s" % filename)
    print("\tOutput file: %s" % out)
    print("")

    if filename is not None:
        print("Always make sure you have the newest language-subtag-registry.txt!")
        registry = io.open(filename, "r", encoding="utf-8")
    else:
        print("Downloading IANA Language Subtag Registry...")
        with closing(urlopen(url)) as reader:
            text = reader.read().decode("utf-8")
        registry = io.open("language-subtag-registry.txt", "w+", encoding="utf-8")
        registry.write(text)
        registry.seek(0)

    print("Processing IANA Language Subtag Registry...")
    with closing(registry) as reg:
        data = readRegistry(reg)

    print("Writing Intl data...")
    with io.open(out, mode="w", encoding="utf-8", newline="") as f:
        println = partial(print, file=f)

        println(u"// Generated by make_intl_data.py. DO NOT EDIT.")
        writeLanguageTagData(println, data, url)

def flines(filepath, encoding="utf-8"):
    """ Open filepath and iterate over its content. """
    with io.open(filepath, mode="r", encoding=encoding) as f:
        for line in f:
            yield line

@total_ordering
class Zone(object):
    """ Time zone with optional file name. """

    def __init__(self, name, filename=""):
        self.name = name
        self.filename = filename
    def __eq__(self, other):
        return hasattr(other, "name") and self.name == other.name
    def __lt__(self, other):
        return self.name < other.name
    def __hash__(self):
        return hash(self.name)
    def __str__(self):
        return self.name
    def __repr__(self):
        return self.name

class TzDataDir(object):
    """ tzdata source from a directory. """

    def __init__(self, obj):
        self.name = partial(os.path.basename, obj)
        self.resolve = partial(os.path.join, obj)
        self.basename = os.path.basename
        self.isfile = os.path.isfile
        self.listdir = partial(os.listdir, obj)
        self.readlines = flines

class TzDataFile(object):
    """ tzdata source from a file (tar or gzipped). """

    def __init__(self, obj):
        self.name = lambda: os.path.splitext(os.path.splitext(os.path.basename(obj))[0])[0]
        self.resolve = obj.getmember
        self.basename = attrgetter("name")
        self.isfile = tarfile.TarInfo.isfile
        self.listdir = obj.getnames
        self.readlines = partial(self._tarlines, obj)

    def _tarlines(self, tar, m):
        with closing(tar.extractfile(m)) as f:
            for line in f:
                yield line.decode("utf-8")

def validateTimeZones(zones, links):
    """ Validate the zone and link entries. """
    linkZones = set(links.keys())
    intersect = linkZones.intersection(zones)
    if intersect:
        raise RuntimeError("Links also present in zones: %s" % intersect)

    zoneNames = {z.name for z in zones}
    linkTargets = set(links.values())
    if not linkTargets.issubset(zoneNames):
        raise RuntimeError("Link targets not found: %s" % linkTargets.difference(zoneNames))

def partition(iterable, *predicates):
    def innerPartition(pred, it):
        it1, it2 = tee(it)
        return (filter(pred, it1), filterfalse(pred, it2))
    if len(predicates) == 0:
        return iterable
    (left, right) = innerPartition(predicates[0], iterable)
    if len(predicates) == 1:
        return (left, right)
    return tuple([left] + list(partition(right, *predicates[1:])))

def listIANAFiles(tzdataDir):
    def isTzFile(d, m, f):
        return m(f) and d.isfile(d.resolve(f))
    return filter(partial(isTzFile, tzdataDir, re.compile("^[a-z0-9]+$").match), tzdataDir.listdir())

def readIANAFiles(tzdataDir, files):
    """ Read all IANA time zone files from the given iterable. """
    nameSyntax = "[\w/+\-]+"
    pZone = re.compile(r"Zone\s+(?P<name>%s)\s+.*" % nameSyntax)
    pLink = re.compile(r"Link\s+(?P<target>%s)\s+(?P<name>%s)(?:\s+#.*)?" % (nameSyntax, nameSyntax))

    def createZone(line, fname):
        match = pZone.match(line)
        name = match.group("name")
        return Zone(name, fname)

    def createLink(line, fname):
        match = pLink.match(line)
        (name, target) = match.group("name", "target")
        return (Zone(name, fname), target)

    zones = set()
    links = dict()
    for filename in files:
        filepath = tzdataDir.resolve(filename)
        for line in tzdataDir.readlines(filepath):
            if line.startswith("Zone"):
                zones.add(createZone(line, filename))
            if line.startswith("Link"):
                (link, target) = createLink(line, filename)
                links[link] = target

    return (zones, links)

def readIANATimeZones(tzdataDir, ignoreBackzone, ignoreFactory):
    """ Read the IANA time zone information from `tzdataDir`. """

    backzoneFiles = {"backzone"}
    (bkfiles, tzfiles) = partition(listIANAFiles(tzdataDir), backzoneFiles.__contains__)

    # Read zone and link infos.
    (zones, links) = readIANAFiles(tzdataDir, tzfiles)
    (backzones, backlinks) = readIANAFiles(tzdataDir, bkfiles)

    # Remove the placeholder time zone "Factory".
    if ignoreFactory:
        zones.remove(Zone("Factory"))

    # Merge with backzone data.
    if not ignoreBackzone:
        zones |= backzones
        links = {name: target for name, target in links.items() if name not in backzones}
        links.update(backlinks)

    validateTimeZones(zones, links)

    return (zones, links)

def readICUResourceFile(filename):
    """ Read an ICU resource file.

        Yields (<table-name>, <startOrEnd>, <value>) for each table.
    """

    numberValue = r"-?\d+"
    stringValue = r'".+?"'
    asVector = lambda val: r"%s(?:\s*,\s*%s)*" % (val, val)
    numberVector = asVector(numberValue)
    stringVector = asVector(stringValue)

    reNumberVector = re.compile(numberVector)
    reStringVector = re.compile(stringVector)
    reNumberValue = re.compile(numberValue)
    reStringValue = re.compile(stringValue)
    def parseValue(value):
        m = reNumberVector.match(value)
        if m:
            return [int(v) for v in reNumberValue.findall(value)]
        m = reStringVector.match(value)
        if m:
            return [v[1:-1] for v in reStringValue.findall(value)]
        raise RuntimeError("unknown value type: %s" % value)

    def extractValue(values):
        if len(values) == 0:
            return None
        if len(values) == 1:
            return values[0]
        return values

    def line(*args):
        maybeMultiComments = r"(?:/\*[^*]*\*/)*"
        maybeSingleComment = r"(?://.*)?"
        lineStart = "^%s" % maybeMultiComments
        lineEnd = "%s\s*%s$" % (maybeMultiComments, maybeSingleComment)
        return re.compile(r"\s*".join(chain([lineStart], args, [lineEnd])))

    tableName = r'(?P<quote>"?)(?P<name>.+?)(?P=quote)'
    tableValue = r"(?P<value>%s|%s)" % (numberVector, stringVector)

    reStartTable = line(tableName, r"\{")
    reEndTable = line(r"\}")
    reSingleValue = line(r",?", tableValue, r",?")
    reCompactTable = line(tableName, r"\{", tableValue, r"\}")
    reEmptyLine = line()

    tables = []
    currentTable = lambda: "|".join(tables)
    values = []
    for line in flines(filename, "utf-8-sig"):
        line = line.strip()
        if line == "":
            continue

        m = reEmptyLine.match(line)
        if m:
            continue

        m = reStartTable.match(line)
        if m:
            assert len(values) == 0
            tables.append(m.group("name"))
            continue

        m = reEndTable.match(line)
        if m:
            yield (currentTable(), extractValue(values))
            tables.pop()
            values = []
            continue

        m = reCompactTable.match(line)
        if m:
            assert len(values) == 0
            tables.append(m.group("name"))
            yield (currentTable(), extractValue(parseValue(m.group("value"))))
            tables.pop()
            continue

        m = reSingleValue.match(line)
        if m and tables:
            values.extend(parseValue(m.group("value")))
            continue

        raise RuntimeError("unknown entry: %s" % line)

def readICUTimeZonesFromTimezoneTypes(icuTzDir):
    """ Read the ICU time zone information from `icuTzDir`/timezoneTypes.txt
        and returns the tuple (zones, links).
    """
    typeMapTimeZoneKey = "timezoneTypes:table(nofallback)|typeMap|timezone|"
    typeAliasTimeZoneKey = "timezoneTypes:table(nofallback)|typeAlias|timezone|"
    toTimeZone = lambda name: Zone(name.replace(":", "/"))

    zones = set()
    links = dict()

    for name, value in readICUResourceFile(os.path.join(icuTzDir, "timezoneTypes.txt")):
        if name.startswith(typeMapTimeZoneKey):
            zones.add(toTimeZone(name[len(typeMapTimeZoneKey):]))
        if name.startswith(typeAliasTimeZoneKey):
            links[toTimeZone(name[len(typeAliasTimeZoneKey):])] = value

    # Remove the ICU placeholder time zone "Etc/Unknown".
    zones.remove(Zone("Etc/Unknown"))

    # tzdata2017c removed the link Canada/East-Saskatchewan -> America/Regina,
    # but it is still present in ICU sources. Manually remove it to keep our
    # tables consistent with IANA.
    del links[Zone("Canada/East-Saskatchewan")]

    validateTimeZones(zones, links)

    return (zones, links)

def readICUTimeZonesFromZoneInfo(icuTzDir, ignoreFactory):
    """ Read the ICU time zone information from `icuTzDir`/zoneinfo64.txt
        and returns the tuple (zones, links).
    """
    zoneKey = "zoneinfo64:table(nofallback)|Zones:array|:table"
    linkKey = "zoneinfo64:table(nofallback)|Zones:array|:int"
    namesKey = "zoneinfo64:table(nofallback)|Names"

    tzId = 0
    tzLinks = dict()
    tzNames = []

    for name, value in readICUResourceFile(os.path.join(icuTzDir, "zoneinfo64.txt")):
        if name == zoneKey:
            tzId += 1
        elif name == linkKey:
            tzLinks[tzId] = int(value)
            tzId += 1
        elif name == namesKey:
            tzNames.extend(value)

    links = {Zone(tzNames[zone]): tzNames[target] for (zone, target) in tzLinks.items()}
    zones = {Zone(v) for v in tzNames if Zone(v) not in links}

    # Remove the ICU placeholder time zone "Etc/Unknown".
    zones.remove(Zone("Etc/Unknown"))

    # tzdata2017c removed the link Canada/East-Saskatchewan -> America/Regina,
    # but it is still present in ICU sources. Manually remove it to keep our
    # tables consistent with IANA.
    del links[Zone("Canada/East-Saskatchewan")]

    # Remove the placeholder time zone "Factory".
    if ignoreFactory:
        zones.remove(Zone("Factory"))

    validateTimeZones(zones, links)

    return (zones, links)

def readICUTimeZones(icuDir, icuTzDir, ignoreFactory):
    # zoneinfo64.txt contains the supported time zones by ICU. This data is
    # generated from tzdata files, it doesn't include "backzone" in stock ICU.
    (zoneinfoZones, zoneinfoLinks) = readICUTimeZonesFromZoneInfo(icuTzDir, ignoreFactory)

    # timezoneTypes.txt contains the canonicalization information for ICU. This
    # data is generated from CLDR files. It includes data about time zones from
    # tzdata's "backzone" file.
    (typesZones, typesLinks) = readICUTimeZonesFromTimezoneTypes(icuTzDir)

    # Information in zoneinfo64 should be a superset of timezoneTypes.
    inZoneInfo64 = lambda zone: zone in zoneinfoZones or zone in zoneinfoLinks

    # Remove legacy ICU time zones from zoneinfo64 data.
    (legacyZones, legacyLinks) = readICULegacyZones(icuDir)
    zoneinfoZones = {zone for zone in zoneinfoZones if zone not in legacyZones}
    zoneinfoLinks = {zone: target for (zone, target) in zoneinfoLinks.items() if zone not in legacyLinks}

    notFoundInZoneInfo64 = [zone for zone in typesZones if not inZoneInfo64(zone)]
    if notFoundInZoneInfo64:
        raise RuntimeError("Missing time zones in zoneinfo64.txt: %s" % notFoundInZoneInfo64)

    notFoundInZoneInfo64 = [zone for zone in typesLinks.keys() if not inZoneInfo64(zone)]
    if notFoundInZoneInfo64:
        raise RuntimeError("Missing time zones in zoneinfo64.txt: %s" % notFoundInZoneInfo64)

    # zoneinfo64.txt only defines the supported time zones by ICU, the canonicalization
    # rules are defined through timezoneTypes.txt. Merge both to get the actual zones
    # and links used by ICU.
    icuZones = set(chain(
                 (zone for zone in zoneinfoZones if zone not in typesLinks),
                 (zone for zone in typesZones)
               ))
    icuLinks = dict(chain(
                 ((zone, target) for (zone, target) in zoneinfoLinks.items() if zone not in typesZones),
                 ((zone, target) for (zone, target) in typesLinks.items())
               ))

    return (icuZones, icuLinks)


def readICULegacyZones(icuDir):
    """ Read the ICU legacy time zones from `icuTzDir`/tools/tzcode/icuzones
        and returns the tuple (zones, links).
    """
    tzdir = TzDataDir(os.path.join(icuDir, "tools/tzcode"))
    (zones, links) = readIANAFiles(tzdir, ["icuzones"])

    # Remove the ICU placeholder time zone "Etc/Unknown".
    zones.remove(Zone("Etc/Unknown"))

    # tzdata2017c removed the link Canada/East-Saskatchewan -> America/Regina,
    # but it is still present in ICU sources. Manually tag it as a legacy time
    # zone so our tables are kept consistent with IANA.
    links[Zone("Canada/East-Saskatchewan")] = "America/Regina"

    return (zones, links)

def icuTzDataVersion(icuTzDir):
    """ Read the ICU time zone version from `icuTzDir`/zoneinfo64.txt. """
    def searchInFile(pattern, f):
        p = re.compile(pattern)
        for line in flines(f, "utf-8-sig"):
            m = p.search(line)
            if m:
                return m.group(1)
        return None

    zoneinfo = os.path.join(icuTzDir, "zoneinfo64.txt")
    if not os.path.isfile(zoneinfo):
        raise RuntimeError("file not found: %s" % zoneinfo)
    version = searchInFile("^//\s+tz version:\s+([0-9]{4}[a-z])$", zoneinfo)
    if version is None:
        raise RuntimeError("%s does not contain a valid tzdata version string" % zoneinfo)
    return version

def findIncorrectICUZones(ianaZones, ianaLinks, icuZones, icuLinks, ignoreBackzone):
    """ Find incorrect ICU zone entries. """
    isIANATimeZone = lambda zone: zone in ianaZones or zone in ianaLinks
    isICUTimeZone = lambda zone: zone in icuZones or zone in icuLinks
    isICULink = lambda zone: zone in icuLinks

    # All IANA zones should be present in ICU.
    missingTimeZones = [zone for zone in ianaZones if not isICUTimeZone(zone)]
    # Normally zones in backzone are also present as links in one of the other
    # time zone files. The only exception to this rule is the Asia/Hanoi time
    # zone, this zone is only present in the backzone file.
    expectedMissing = [] if ignoreBackzone else [Zone("Asia/Hanoi")]
    if missingTimeZones != expectedMissing:
        raise RuntimeError("Not all zones are present in ICU, did you forget "
                           "to run intl/update-tzdata.sh? %s" % missingTimeZones)

    # Zones which are only present in ICU?
    additionalTimeZones = [zone for zone in icuZones if not isIANATimeZone(zone)]
    if additionalTimeZones:
        raise RuntimeError("Additional zones present in ICU, did you forget "
                           "to run intl/update-tzdata.sh? %s" % additionalTimeZones)

    # Zones which are marked as links in ICU.
    result = ((zone, icuLinks[zone]) for zone in ianaZones if isICULink(zone))

    # Remove unnecessary UTC mappings.
    utcnames = ["Etc/UTC", "Etc/UCT", "Etc/GMT"]
    result = ((zone, target) for (zone, target) in result if zone.name not in utcnames)

    return sorted(result, key=itemgetter(0))

def findIncorrectICULinks(ianaZones, ianaLinks, icuZones, icuLinks):
    """ Find incorrect ICU link entries. """
    isIANATimeZone = lambda zone: zone in ianaZones or zone in ianaLinks
    isICUTimeZone = lambda zone: zone in icuZones or zone in icuLinks
    isICULink = lambda zone: zone in icuLinks
    isICUZone = lambda zone: zone in icuZones

    # All links should be present in ICU.
    missingTimeZones = [zone for zone in ianaLinks.keys() if not isICUTimeZone(zone)]
    if missingTimeZones:
        raise RuntimeError("Not all zones are present in ICU, did you forget "
                           "to run intl/update-tzdata.sh? %s" % missingTimeZones)

    # Links which are only present in ICU?
    additionalTimeZones = [zone for zone in icuLinks.keys() if not isIANATimeZone(zone)]
    if additionalTimeZones:
        raise RuntimeError("Additional links present in ICU, did you forget "
                           "to run intl/update-tzdata.sh? %s" % additionalTimeZones)

    result = chain(
        # IANA links which have a different target in ICU.
        ((zone, target, icuLinks[zone]) for (zone, target) in ianaLinks.items() if isICULink(zone) and target != icuLinks[zone]),

        # IANA links which are zones in ICU.
        ((zone, target, zone.name) for (zone, target) in ianaLinks.items() if isICUZone(zone))
    )

    # Remove unnecessary UTC mappings.
    utcnames = ["Etc/UTC", "Etc/UCT", "Etc/GMT"]
    result = ((zone, target, icuTarget)
              for (zone, target, icuTarget) in result
              if target not in utcnames or icuTarget not in utcnames)

    return sorted(result, key=itemgetter(0))

generatedFileWarning = u"// Generated by make_intl_data.py. DO NOT EDIT."
tzdataVersionComment = u"// tzdata version = {0}"

def processTimeZones(tzdataDir, icuDir, icuTzDir, version, ignoreBackzone, ignoreFactory, out):
    """ Read the time zone info and create a new time zone cpp file. """
    print("Processing tzdata mapping...")
    (ianaZones, ianaLinks) = readIANATimeZones(tzdataDir, ignoreBackzone, ignoreFactory)
    (icuZones, icuLinks) = readICUTimeZones(icuDir, icuTzDir, ignoreFactory)
    (legacyZones, legacyLinks) = readICULegacyZones(icuDir)

    incorrectZones = findIncorrectICUZones(ianaZones, ianaLinks, icuZones, icuLinks, ignoreBackzone)
    if not incorrectZones:
        print("<<< No incorrect ICU time zones found, please update Intl.js! >>>")
        print("<<< Maybe https://ssl.icu-project.org/trac/ticket/12044 was fixed? >>>")

    incorrectLinks = findIncorrectICULinks(ianaZones, ianaLinks, icuZones, icuLinks)
    if not incorrectLinks:
        print("<<< No incorrect ICU time zone links found, please update Intl.js! >>>")
        print("<<< Maybe https://ssl.icu-project.org/trac/ticket/12044 was fixed? >>>")

    print("Writing Intl tzdata file...")
    with io.open(out, mode="w", encoding="utf-8", newline="") as f:
        println = partial(print, file=f)

        println(generatedFileWarning)
        println(tzdataVersionComment.format(version))
        println(u"")

        println(u"#ifndef builtin_intl_TimeZoneDataGenerated_h")
        println(u"#define builtin_intl_TimeZoneDataGenerated_h")
        println(u"")

        println(u"namespace js {")
        println(u"namespace timezone {")
        println(u"")

        println(u"// Format:")
        println(u'// "ZoneName" // ICU-Name [time zone file]')
        println(u"const char* const ianaZonesTreatedAsLinksByICU[] = {")
        for (zone, icuZone) in incorrectZones:
            println(u'    "%s", // %s [%s]' % (zone, icuZone, zone.filename))
        println(u"};")
        println(u"")

        println(u"// Format:")
        println(u'// "LinkName", "Target" // ICU-Target [time zone file]')
        println(u"struct LinkAndTarget")
        println(u"{")
        println(u"    const char* const link;")
        println(u"    const char* const target;")
        println(u"};")
        println(u"")
        println(u"const LinkAndTarget ianaLinksCanonicalizedDifferentlyByICU[] = {")
        for (zone, target, icuTarget) in incorrectLinks:
            println(u'    { "%s", "%s" }, // %s [%s]' % (zone, target, icuTarget, zone.filename))
        println(u"};")
        println(u"")

        println(u"// Legacy ICU time zones, these are not valid IANA time zone names. We also")
        println(u"// disallow the old and deprecated System V time zones.")
        println(u"// https://ssl.icu-project.org/repos/icu/trunk/icu4c/source/tools/tzcode/icuzones")
        println(u"const char* const legacyICUTimeZones[] = {")
        for zone in chain(sorted(legacyLinks.keys()), sorted(legacyZones)):
            println(u'    "%s",' % zone)
        println(u"};")
        println(u"")

        println(u"} // namespace timezone")
        println(u"} // namespace js")
        println(u"")
        println(u"#endif /* builtin_intl_TimeZoneDataGenerated_h */")

def updateBackzoneLinks(tzdataDir, links):
    withZone = lambda fn: lambda zone_target: fn(zone_target[0])

    (backzoneZones, backzoneLinks) = readIANAFiles(tzdataDir, ["backzone"])
    (stableZones, updatedLinks, updatedZones) = partition(
        links.items(),
        # Link not changed in backzone.
        withZone(lambda zone: zone not in backzoneLinks and zone not in backzoneZones),
        # Link has a new target.
        withZone(lambda zone: zone in backzoneLinks),
    )
    # Keep stable zones and links with updated target.
    return dict(chain(
                stableZones,
                map(withZone(lambda zone: (zone, backzoneLinks[zone])), updatedLinks)
           ))

def generateTzDataLinkTestContent(testDir, version, fileName, description, links):
    with io.open(os.path.join(testDir, fileName), mode="w", encoding="utf-8", newline="") as f:
        println = partial(print, file=f)

        println(u'// |reftest| skip-if(!this.hasOwnProperty("Intl"))')
        println(u"")
        println(generatedFileWarning)
        println(tzdataVersionComment.format(version))
        println(u"""
const tzMapper = [
    x => x,
    x => x.toUpperCase(),
    x => x.toLowerCase(),
];
""")

        println(description)
        println(u"const links = {")
        for (zone, target) in sorted(links, key=itemgetter(0)):
            println(u'    "%s": "%s",' % (zone, target))
        println(u"};")

        println(u"""
for (let [linkName, target] of Object.entries(links)) {
    if (target === "Etc/UTC" || target === "Etc/GMT")
        target = "UTC";

    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(undefined, {timeZone: map(linkName)});
        let resolvedTimeZone = dtf.resolvedOptions().timeZone;
        assertEq(resolvedTimeZone, target, `${linkName} -> ${target}`);
    }
}
""")
        println(u"""
if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
""")

def generateTzDataTestBackwardLinks(tzdataDir, version, ignoreBackzone, testDir):
    (zones, links) = readIANAFiles(tzdataDir, ["backward"])
    assert len(zones) == 0

    if not ignoreBackzone:
        links = updateBackzoneLinks(tzdataDir, links)

    generateTzDataLinkTestContent(
        testDir, version,
        "timeZone_backward_links.js",
        u"// Link names derived from IANA Time Zone Database, backward file.",
        links.items()
    )

def generateTzDataTestNotBackwardLinks(tzdataDir, version, ignoreBackzone, testDir):
    tzfiles = filterfalse({"backward", "backzone"}.__contains__, listIANAFiles(tzdataDir))
    (zones, links) = readIANAFiles(tzdataDir, tzfiles)

    if not ignoreBackzone:
        links = updateBackzoneLinks(tzdataDir, links)

    generateTzDataLinkTestContent(
        testDir, version,
        "timeZone_notbackward_links.js",
        u"// Link names derived from IANA Time Zone Database, excluding backward file.",
        links.items()
    )

def generateTzDataTestBackzone(tzdataDir, version, ignoreBackzone, testDir):
    backzoneFiles = {"backzone"}
    (bkfiles, tzfiles) = partition(listIANAFiles(tzdataDir), backzoneFiles.__contains__)

    # Read zone and link infos.
    (zones, links) = readIANAFiles(tzdataDir, tzfiles)
    (backzones, backlinks) = readIANAFiles(tzdataDir, bkfiles)

    if not ignoreBackzone:
        comment=u"""\
// This file was generated with historical, pre-1970 backzone information
// respected. Therefore, every zone key listed below is its own Zone, not
// a Link to a modern-day target as IANA ignoring backzones would say.

"""
    else:
        comment=u"""\
// This file was generated while ignoring historical, pre-1970 backzone
// information. Therefore, every zone key listed below is part of a Link
// whose target is the corresponding value.

"""

    generateTzDataLinkTestContent(
        testDir, version,
        "timeZone_backzone.js",
        comment + u"// Backzone zones derived from IANA Time Zone Database.",
        ((zone, zone if not ignoreBackzone else links[zone]) for zone in backzones if zone in links)
    )

def generateTzDataTestBackzoneLinks(tzdataDir, version, ignoreBackzone, testDir):
    backzoneFiles = {"backzone"}
    (bkfiles, tzfiles) = partition(listIANAFiles(tzdataDir), backzoneFiles.__contains__)

    # Read zone and link infos.
    (zones, links) = readIANAFiles(tzdataDir, tzfiles)
    (backzones, backlinks) = readIANAFiles(tzdataDir, bkfiles)

    if not ignoreBackzone:
        comment=u"""\
// This file was generated with historical, pre-1970 backzone information
// respected. Therefore, every zone key listed below points to a target
// in the backzone file and not to its modern-day target as IANA ignoring
// backzones would say.

"""
    else:
        comment=u"""\
// This file was generated while ignoring historical, pre-1970 backzone
// information. Therefore, every zone key listed below is part of a Link
// whose target is the corresponding value ignoring any backzone entries.

"""

    generateTzDataLinkTestContent(
        testDir, version,
        "timeZone_backzone_links.js",
        comment +  u"// Backzone links derived from IANA Time Zone Database.",
        ((zone, target if not ignoreBackzone else links[zone]) for (zone, target) in backlinks.items())
    )

def generateTzDataTests(tzdataDir, version, ignoreBackzone, testDir):
    generateTzDataTestBackwardLinks(tzdataDir, version, ignoreBackzone, testDir)
    generateTzDataTestNotBackwardLinks(tzdataDir, version, ignoreBackzone, testDir)
    generateTzDataTestBackzone(tzdataDir, version, ignoreBackzone, testDir)
    generateTzDataTestBackzoneLinks(tzdataDir, version, ignoreBackzone, testDir)

def updateTzdata(topsrcdir, args):
    """ Update the time zone cpp file. """

    icuDir = os.path.join(topsrcdir, "intl/icu/source")
    if not os.path.isdir(icuDir):
        raise RuntimeError("not a directory: %s" % icuDir)

    icuTzDir = os.path.join(topsrcdir, "intl/tzdata/source")
    if not os.path.isdir(icuTzDir):
        raise RuntimeError("not a directory: %s" % icuTzDir)

    dateTimeFormatTestDir = os.path.join(topsrcdir, "js/src/tests/non262/Intl/DateTimeFormat")
    if not os.path.isdir(dateTimeFormatTestDir):
        raise RuntimeError("not a directory: %s" % dateTimeFormatTestDir)

    tzDir = args.tz
    if tzDir is not None and not (os.path.isdir(tzDir) or os.path.isfile(tzDir)):
        raise RuntimeError("not a directory or file: %s" % tzDir)
    ignoreBackzone = args.ignore_backzone
    # TODO: Accept or ignore the placeholder time zone "Factory"?
    ignoreFactory = False
    out = args.out

    version = icuTzDataVersion(icuTzDir)
    url = "https://www.iana.org/time-zones/repository/releases/tzdata%s.tar.gz" % version

    print("Arguments:")
    print("\ttzdata version: %s" % version)
    print("\ttzdata URL: %s" % url)
    print("\ttzdata directory|file: %s" % tzDir)
    print("\tICU directory: %s" % icuDir)
    print("\tICU timezone directory: %s" % icuTzDir)
    print("\tIgnore backzone file: %s" % ignoreBackzone)
    print("\tOutput file: %s" % out)
    print("")

    def updateFrom(f):
        if os.path.isfile(f) and tarfile.is_tarfile(f):
            with tarfile.open(f, "r:*") as tar:
                processTimeZones(TzDataFile(tar), icuDir, icuTzDir, version, ignoreBackzone, ignoreFactory, out)
                generateTzDataTests(TzDataFile(tar), version, ignoreBackzone, dateTimeFormatTestDir)
        elif os.path.isdir(f):
            processTimeZones(TzDataDir(f), icuDir, icuTzDir, version, ignoreBackzone, ignoreFactory, out)
            generateTzDataTests(TzDataDir(f), version, ignoreBackzone, dateTimeFormatTestDir)
        else:
            raise RuntimeError("unknown format")

    if tzDir is None:
        print("Downloading tzdata file...")
        with closing(urlopen(url)) as tzfile:
            fname = urlsplit(tzfile.geturl()).path.split("/")[-1]
            with tempfile.NamedTemporaryFile(suffix=fname) as tztmpfile:
                print("File stored in %s" % tztmpfile.name)
                tztmpfile.write(tzfile.read())
                tztmpfile.flush()
                updateFrom(tztmpfile.name)
    else:
        updateFrom(tzDir)

def readCurrencyFile(tree):
    reCurrency = re.compile(r"^[A-Z]{3}$")
    reIntMinorUnits = re.compile(r"^\d+$")

    for country in tree.iterfind(".//CcyNtry"):
        # Skip entry if no currency information is available.
        currency = country.findtext("Ccy")
        if currency is None:
            continue
        assert reCurrency.match(currency)

        minorUnits = country.findtext("CcyMnrUnts")
        assert minorUnits is not None

        # Skip all entries without minorUnits or which use the default minorUnits.
        if reIntMinorUnits.match(minorUnits) and int(minorUnits) != 2:
            currencyName = country.findtext("CcyNm")
            countryName = country.findtext("CtryNm")
            yield (currency, int(minorUnits), currencyName, countryName)

def writeCurrencyFile(published, currencies, out):
    with io.open(out, mode="w", encoding="utf-8", newline="") as f:
        println = partial(print, file=f)

        println(generatedFileWarning)
        println(u"// Version: {}".format(published))

        println(u"""
/**
 * Mapping from currency codes to the number of decimal digits used for them.
 * Default is 2 digits.
 *
 * Spec: ISO 4217 Currency and Funds Code List.
 * http://www.currency-iso.org/en/home/tables/table-a1.html
 */""")
        println(u"var currencyDigits = {")
        for (currency, entries) in groupby(sorted(currencies, key=itemgetter(0)), itemgetter(0)):
            for (_, minorUnits, currencyName, countryName) in entries:
                println(u"    // {} ({})".format(currencyName, countryName))
            println(u"    {}: {},".format(currency, minorUnits))
        println(u"};")

def updateCurrency(topsrcdir, args):
    """ Update the CurrencyDataGenerated.js file. """
    import xml.etree.ElementTree as ET
    from random import randint

    url = args.url
    out = args.out
    filename = args.file

    print("Arguments:")
    print("\tDownload url: %s" % url)
    print("\tLocal currency file: %s" % filename)
    print("\tOutput file: %s" % out)
    print("")

    def updateFrom(currencyFile):
        print("Processing currency code list file...")
        tree = ET.parse(currencyFile)
        published = tree.getroot().attrib["Pblshd"]
        currencies = readCurrencyFile(tree)

        print("Writing CurrencyData file...")
        writeCurrencyFile(published, currencies, out)

    if filename is not None:
        print("Always make sure you have the newest currency code list file!")
        updateFrom(filename)
    else:
        print("Downloading currency & funds code list...")
        request = UrlRequest(url)
        request.add_header("User-agent", "Mozilla/5.0 (Mobile; rv:{0}.0) Gecko/{0}.0 Firefox/{0}.0".format(randint(1, 999)))
        with closing(urlopen(request)) as currencyFile:
            fname = urlsplit(currencyFile.geturl()).path.split("/")[-1]
            with tempfile.NamedTemporaryFile(suffix=fname) as currencyTmpFile:
                print("File stored in %s" % currencyTmpFile.name)
                currencyTmpFile.write(currencyFile.read())
                currencyTmpFile.flush()
                updateFrom(currencyTmpFile.name)

if __name__ == "__main__":
    import argparse

    # This script must reside in js/src/builtin/intl to work correctly.
    (thisDir, thisFile) = os.path.split(os.path.abspath(sys.argv[0]))
    dirPaths = os.path.normpath(thisDir).split(os.sep)
    if "/".join(dirPaths[-4:]) != "js/src/builtin/intl":
        raise RuntimeError("%s must reside in js/src/builtin/intl" % sys.argv[0])
    topsrcdir = "/".join(dirPaths[:-4])

    def EnsureHttps(v):
        if not v.startswith("https:"):
            raise argparse.ArgumentTypeError("URL protocol must be https: " % v)
        return v

    parser = argparse.ArgumentParser(description="Update intl data.")
    subparsers = parser.add_subparsers(help="Select update mode")

    parser_tags = subparsers.add_parser("langtags",
                                        help="Update language-subtag-registry")
    parser_tags.add_argument("--url",
                             metavar="URL",
                             default="https://www.iana.org/assignments/language-subtag-registry",
                             type=EnsureHttps,
                             help="Download url for language-subtag-registry.txt (default: %(default)s)")
    parser_tags.add_argument("--out",
                             default="LangTagMappingsGenerated.js",
                             help="Output file (default: %(default)s)")
    parser_tags.add_argument("file",
                             nargs="?",
                             help="Local language-subtag-registry.txt file, if omitted uses <URL>")
    parser_tags.set_defaults(func=updateLangTags)

    parser_tz = subparsers.add_parser("tzdata", help="Update tzdata")
    parser_tz.add_argument("--tz",
                           help="Local tzdata directory or file, if omitted downloads tzdata "
                                "distribution from https://www.iana.org/time-zones/")
    # ICU doesn't include the backzone file by default, but we still like to
    # use the backzone time zone names to avoid user confusion. This does lead
    # to formatting "historic" dates (pre-1970 era) with the wrong time zone,
    # but that's probably acceptable for now.
    parser_tz.add_argument("--ignore-backzone",
                           action="store_true",
                           help="Ignore tzdata's 'backzone' file. Can be enabled to generate more "
                                "accurate time zone canonicalization reflecting the actual time "
                                "zones as used by ICU.")
    parser_tz.add_argument("--out",
                           default="TimeZoneDataGenerated.h",
                           help="Output file (default: %(default)s)")
    parser_tz.set_defaults(func=partial(updateTzdata, topsrcdir))


    parser_currency = subparsers.add_parser("currency", help="Update currency digits mapping")
    parser_currency.add_argument("--url",
                                 metavar="URL",
                                 default="https://www.currency-iso.org/dam/downloads/lists/list_one.xml",
                                 type=EnsureHttps,
                                 help="Download url for the currency & funds code list (default: "
                                      "%(default)s)")
    parser_currency.add_argument("--out",
                                 default="CurrencyDataGenerated.js",
                                 help="Output file (default: %(default)s)")
    parser_currency.add_argument("file",
                                 nargs="?",
                                 help="Local currency code list file, if omitted uses <URL>")
    parser_currency.set_defaults(func=partial(updateCurrency, topsrcdir))

    args = parser.parse_args()
    args.func(args)
