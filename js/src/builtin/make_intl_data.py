#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" Usage:
    make_intl_data.py langtags [language-subtag-registry.txt]
    make_intl_data.py tzdata --icu=$MOZ/intl/icu/source \\
                             --icutz=$MOZ/intl/tzdata/tz2016f \\
                             --version=2016f

    Target "langtags":
    This script extracts information about mappings between deprecated and
    current BCP 47 language tags from the IANA Language Subtag Registry and
    converts it to JavaScript object definitions in IntlData.js. The definitions
    are used in Intl.js.

    The IANA Language Subtag Registry is imported from
    http://www.iana.org/assignments/language-subtag-registry
    and uses the syntax specified in
    http://tools.ietf.org/html/rfc5646#section-3


    Target "tzdata":
    This script computes which time zone informations are not up-to-date in ICU
    and provides the necessary mappings to workaround this problem.
    http://bugs.icu-project.org/trac/ticket/12044
"""

from __future__ import print_function
import os
import re
import io
import codecs
import tarfile
import tempfile
import urllib2
import urlparse
from contextlib import closing
from functools import partial
from itertools import chain, ifilter, ifilterfalse, imap, tee
from operator import attrgetter, itemgetter

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
        - langTagMappings: mappings from complete language tags to preferred
          complete language tags
        - langSubtagMappings: mappings from subtags to preferred subtags
        - extlangMappings: mappings from extlang subtags to preferred subtags,
          with prefix to be removed
        Returns these three mappings as dictionaries, along with the registry's
        file date.

        We also check that mappings for language subtags don't affect extlang
        subtags and vice versa, so that CanonicalizeLanguageTag doesn't have
        to separate them for processing. Region codes are separated by case,
        and script codes by length, so they're unproblematic.
    """
    langTagMappings = {}
    langSubtagMappings = {}
    extlangMappings = {}
    languageSubtags = set()
    extlangSubtags = set()

    for record in readRegistryRecord(registry):
        if "File-Date" in record:
            fileDate = record["File-Date"]
            continue

        if record["Type"] == "grandfathered":
            # Grandfathered tags don't use standard syntax, so
            # CanonicalizeLanguageTag expects the mapping table to provide
            # the final form for all.
            # For langTagMappings, keys must be in lower case; values in
            # the case used in the registry.
            tag = record["Tag"]
            if "Preferred-Value" in record:
                langTagMappings[tag.lower()] = record["Preferred-Value"]
            else:
                langTagMappings[tag.lower()] = tag
        elif record["Type"] == "redundant":
            # For langTagMappings, keys must be in lower case; values in
            # the case used in the registry.
            if "Preferred-Value" in record:
                langTagMappings[record["Tag"].lower()] = record["Preferred-Value"]
        elif record["Type"] in ("language", "script", "region", "variant"):
            # For langSubtagMappings, keys and values must be in the case used
            # in the registry.
            subtag = record["Subtag"]
            if record["Type"] == "language":
                languageSubtags.add(subtag)
            if "Preferred-Value" in record:
                if subtag == "heploc":
                    # The entry for heploc is unique in its complexity; handle
                    # it as special case below.
                    continue
                if "Prefix" in record:
                    # This might indicate another heploc-like complex case.
                    raise Exception("Please evaluate: subtag mapping with prefix value.")
                langSubtagMappings[subtag] = record["Preferred-Value"]
        elif record["Type"] == "extlang":
            # For extlangMappings, keys must be in the case used in the
            # registry; values are records with the preferred value and the
            # prefix to be removed.
            subtag = record["Subtag"]
            extlangSubtags.add(subtag)
            if "Preferred-Value" in record:
                preferred = record["Preferred-Value"]
                prefix = record["Prefix"]
                extlangMappings[subtag] = {"preferred": preferred, "prefix": prefix}
        else:
            # No other types are allowed by
            # http://tools.ietf.org/html/rfc5646#section-3.1.3
            assert False, "Unrecognized Type: {0}".format(record["Type"])

    # Check that mappings for language subtags and extlang subtags don't affect
    # each other.
    for lang in languageSubtags:
        if lang in extlangMappings and extlangMappings[lang]["preferred"] != lang:
            raise Exception("Conflict: lang with extlang mapping: " + lang)
    for extlang in extlangSubtags:
        if extlang in langSubtagMappings:
            raise Exception("Conflict: extlang with lang mapping: " + extlang)

    # Special case for heploc.
    langTagMappings["ja-latn-hepburn-heploc"] = "ja-Latn-alalc97"

    return {"fileDate": fileDate,
            "langTagMappings": langTagMappings,
            "langSubtagMappings": langSubtagMappings,
            "extlangMappings": extlangMappings}


def writeMappingsVar(intlData, dict, name, description, fileDate, url):
    """ Writes a variable definition with a mapping table to file intlData.

        Writes the contents of dictionary dict to file intlData with the given
        variable name and a comment with description, fileDate, and URL.
    """
    intlData.write("\n")
    intlData.write("// {0}.\n".format(description))
    intlData.write("// Derived from IANA Language Subtag Registry, file date {0}.\n".format(fileDate))
    intlData.write("// {0}\n".format(url))
    intlData.write("var {0} = {{\n".format(name))
    keys = sorted(dict)
    for key in keys:
        if isinstance(dict[key], basestring):
            value = '"{0}"'.format(dict[key])
        else:
            preferred = dict[key]["preferred"]
            prefix = dict[key]["prefix"]
            value = '{{preferred: "{0}", prefix: "{1}"}}'.format(preferred, prefix)
        intlData.write('    "{0}": {1},\n'.format(key, value))
    intlData.write("};\n")


def writeLanguageTagData(intlData, fileDate, url, langTagMappings, langSubtagMappings, extlangMappings):
    """ Writes the language tag data to the Intl data file. """
    writeMappingsVar(intlData, langTagMappings, "langTagMappings",
                     "Mappings from complete tags to preferred values", fileDate, url)
    writeMappingsVar(intlData, langSubtagMappings, "langSubtagMappings",
                     "Mappings from non-extlang subtags to preferred values", fileDate, url)
    writeMappingsVar(intlData, extlangMappings, "extlangMappings",
                     "Mappings from extlang subtags to preferred values", fileDate, url)

def updateLangTags(args):
    """ Update the IntlData.js file. """
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
        registry = codecs.open(filename, "r", encoding="utf-8")
    else:
        print("Downloading IANA Language Subtag Registry...")
        with closing(urllib2.urlopen(url)) as reader:
            text = reader.read().decode("utf-8")
        registry = codecs.open("language-subtag-registry.txt", "w+", encoding="utf-8")
        registry.write(text)
        registry.seek(0)

    print("Processing IANA Language Subtag Registry...")
    with closing(registry) as reg:
        data = readRegistry(reg)
    fileDate = data["fileDate"]
    langTagMappings = data["langTagMappings"]
    langSubtagMappings = data["langSubtagMappings"]
    extlangMappings = data["extlangMappings"]

    print("Writing Intl data...")
    with codecs.open(out, "w", encoding="utf-8") as intlData:
        intlData.write("// Generated by make_intl_data.py. DO NOT EDIT.\n")
        writeLanguageTagData(intlData, fileDate, url, langTagMappings, langSubtagMappings, extlangMappings)

def flines(filepath, encoding="utf-8"):
    """ Open filepath and iterate over its content. """
    with io.open(filepath, mode="r", encoding=encoding) as f:
        for line in f:
            yield line

class Zone:
    """ Time zone with optional file name. """

    def __init__(self, name, filename=""):
        self.name = name
        self.filename = filename
    def __eq__(self, other):
        return hasattr(other, "name") and self.name == other.name
    def __cmp__(self, other):
        if self.name == other.name:
            return 0
        if self.name < other.name:
            return -1
        return 1
    def __hash__(self):
        return hash(self.name)
    def __str__(self):
        return self.name
    def __repr__(self):
        return self.name

class TzDataDir:
    """ tzdata source from a directory. """

    def __init__(self, obj):
        self.name = partial(os.path.basename, obj)
        self.resolve = partial(os.path.join, obj)
        self.basename = os.path.basename
        self.isfile = os.path.isfile
        self.listdir = partial(os.listdir, obj)
        self.readlines = flines

class TzDataFile:
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
            for line in codecs.EncodedFile(f, "utf-8"):
                yield line

def validateTimeZones(zones, links):
    """ Validate the zone and link entries. """
    linkZones = set(links.viewkeys())
    intersect = linkZones.intersection(zones)
    if intersect:
        raise RuntimeError("Links also present in zones: %s" % intersect)

    zoneNames = set(z.name for z in zones)
    linkTargets = set(links.viewvalues())
    if not linkTargets.issubset(zoneNames):
        raise RuntimeError("Link targets not found: %s" % linkTargets.difference(zoneNames))

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

def readIANATimeZones(tzdataDir):
    """ Read the IANA time zone information from `tzdataDir`. """
    def partition(pred, it):
        it1, it2 = tee(it)
        return (ifilterfalse(pred, it1), ifilter(pred, it2))
    def isTzFile(d, m, f):
        return m(f) and d.isfile(d.resolve(f))

    backzoneFiles = {"backzone"}
    (tzfiles, bkfiles) = partition(
        backzoneFiles.__contains__,
        ifilter(partial(isTzFile, tzdataDir, re.compile("^[a-z0-9]+$").match), tzdataDir.listdir())
    )

    # Read zone and link infos.
    (zones, links) = readIANAFiles(tzdataDir, tzfiles)
    (backzones, backlinks) = readIANAFiles(tzdataDir, bkfiles)

    # Merge with backzone data.
    zones |= backzones
    zones.remove(Zone("Factory"))
    links = {name: target for name, target in links.iteritems() if name not in backzones}
    links.update(backlinks)

    validateTimeZones(zones, links)

    return (zones, links)

def readICUResourceFile(filename):
    """ Read an ICU resource file.

        Returns ("table", <table-name>) when a table starts/ends.
        Returns ("entry", <data>) for each table entry.
    """
    p = re.compile(r"^(?P<name>.+)\{$")
    q = re.compile(r'^(?P<quote>"?)(?P<key>.+)(?P=quote)\{"(?P<value>.+)"\}$')

    tables = []
    for line in flines(filename, "utf-8-sig"):
        line = line.strip()
        if line == "" or line.startswith("//"):
            continue
        m = p.match(line)
        if m:
            tables.append(m.group("name"))
            yield ("table", m.group("name"))
            continue
        if line == "}":
            yield ("table", tables.pop())
            continue
        m = q.match(line)
        if not m:
            raise RuntimeError("unknown entry: %s" % line)
        yield ("entry", (m.group("key"), m.group("value")))

def readICUTimeZones(icuTzDir):
    """ Read the ICU time zone information from `icuTzDir`/timezoneTypes.txt
        and returns the tuple (zones, links).
    """
    inTypeMap = False
    inTypeMapTimezone = False
    inTypeAlias = False
    inTypeAliasTimezone = False
    toTimeZone = lambda name: Zone(name.replace(":", "/"))

    timezones = set()
    aliases = dict()
    for kind, value in readICUResourceFile(os.path.join(icuTzDir, "timezoneTypes.txt")):
        if kind == "table":
            if value == "typeMap":
                inTypeMap = not inTypeMap
            elif inTypeMap and value == "timezone":
                inTypeMapTimezone = not inTypeMapTimezone
            elif value == "typeAlias":
                inTypeAlias = not inTypeAlias
            elif inTypeAlias and value == "timezone":
                inTypeAliasTimezone = not inTypeAliasTimezone
            continue
        (fst, snd) = value
        if inTypeMapTimezone:
            timezones.add(toTimeZone(fst))
        if inTypeAliasTimezone:
            aliases[toTimeZone(fst)] = snd

    # Remove the ICU placeholder time zone "Etc/Unknown".
    timezones.remove(Zone("Etc/Unknown"))

    validateTimeZones(timezones, aliases)

    return (timezones, aliases)

def readICULegacyZones(icuDir):
    """ Read the ICU legacy time zones from `icuTzDir`/tools/tzcode/icuzones
        and returns the tuple (zones, links).
    """
    tzdir = TzDataDir(os.path.join(icuDir, "tools/tzcode"))
    (zones, links) = readIANAFiles(tzdir, ["icuzones"])

    # Remove the ICU placeholder time zone "Etc/Unknown".
    zones.remove(Zone("Etc/Unknown"))

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
    if os.path.isfile(zoneinfo):
        version = searchInFile("^//\s+tz version:\s+([0-9]{4}[a-z])$", zoneinfo)
    else:
        version = None
    return version

def findIncorrectICUZones(zones, links, timezones, aliases):
    """ Find incorrect ICU zone entries. """
    isIANATimeZone = lambda zone: zone in zones or zone in links
    isICUTimeZone = lambda zone: zone in timezones or zone in aliases
    isICULink = lambda zone: zone in aliases

    # Zones which aren't present in ICU.
    missingTimeZones = ((zone, "<not present>") for zone in zones if not isICUTimeZone(zone))

    # Zones which are marked as links in ICU.
    incorrectMapping = ((zone, aliases[zone]) for zone in zones if isICULink(zone))

    # Zones which are only present in ICU?
    removedTimeZones = [zone for zone in timezones if not isIANATimeZone(zone)]
    if removedTimeZones:
        raise RuntimeError("Removed zones? %s" % removedTimeZones)

    result = chain(missingTimeZones, incorrectMapping)

    # Remove unnecessary UTC mappings.
    utcnames = ["Etc/UTC", "Etc/UCT", "Etc/GMT"]
    result = ifilterfalse(lambda (zone, alias): zone.name in utcnames, result)

    return sorted(result, key=itemgetter(0))

def findIncorrectICULinks(zones, links, timezones, aliases):
    """ Find incorrect ICU link entries. """
    isIANATimeZone = lambda zone: zone in zones or zone in links
    isICUTimeZone = lambda zone: zone in timezones or zone in aliases
    isICULink = lambda zone: zone in aliases
    isICUZone = lambda zone: zone in timezones

    # Links which aren't present in ICU.
    missingTimeZones = ((zone, target, "<not present>") for (zone, target) in links.iteritems() if not isICUTimeZone(zone))

    # Links which have a different target in ICU.
    incorrectMapping = ((zone, target, aliases[zone]) for (zone, target) in links.iteritems() if isICULink(zone) and target != aliases[zone])

    # Links which are zones in ICU.
    incorrectMapping2 = ((zone, target, zone.name) for (zone, target) in links.iteritems() if isICUZone(zone))

    # Links which are only present in ICU?
    removedTimeZones = [zone for zone in aliases.iterkeys() if not isIANATimeZone(zone)]
    if removedTimeZones:
        raise RuntimeError("Removed zones? %s" % removedTimeZones)

    result = chain(missingTimeZones, incorrectMapping, incorrectMapping2)

    # Remove unnecessary UTC mappings.
    utcnames = ["Etc/UTC", "Etc/UCT", "Etc/GMT"]
    result = ifilterfalse(lambda (zone, target, alias): target in utcnames and alias in utcnames, result)

    return sorted(result, key=itemgetter(0))

def processTimeZones(tzdataDir, icuDir, icuTzDir, tzversion, out):
    """ Read the time zone info and create a new IntlTzData.js file. """
    print("Processing tzdata mapping...")
    (zones, links) = readIANATimeZones(tzdataDir)
    (timezones, aliases) = readICUTimeZones(icuTzDir)
    (legacyzones, legacylinks) = readICULegacyZones(icuDir)
    icuversion = icuTzDataVersion(icuTzDir)

    incorrectZones = findIncorrectICUZones(zones, links, timezones, aliases)
    if not incorrectZones:
        print("<<< No incorrect ICU time zones found, please update Intl.js! >>>")
        print("<<< Maybe http://bugs.icu-project.org/trac/ticket/12044 was fixed? >>>")

    incorrectLinks = findIncorrectICULinks(zones, links, timezones, aliases)
    if not incorrectLinks:
        print("<<< No incorrect ICU time zone links found, please update Intl.js! >>>")
        print("<<< Maybe http://bugs.icu-project.org/trac/ticket/12044 was fixed? >>>")

    print("Writing Intl tzdata file...")
    with io.open(out, mode="w", encoding="utf-8", newline="") as f:
        println = partial(print, file=f)

        println(u"// Generated by make_intl_data.py. DO NOT EDIT.")
        println(u"// tzdata version = %s" % tzversion)
        println(u"// ICU tzdata version = %s" % icuversion)
        println(u"")

        println(u"// Format:")
        println(u'// "ZoneName": true // ICU-Name [time zone file]')
        println(u"var tzZoneNamesNonICU = {")
        for (zone, alias) in incorrectZones:
            println(u'    "%s": true, // %s [%s]' % (zone, alias, zone.filename))
        println(u"};")
        println(u"")

        println(u"// Format:")
        println(u'// "LinkName": "Target" // ICU-Target [time zone file]')
        println(u"var tzLinkNamesNonICU = {")
        for (zone, target, alias) in incorrectLinks:
            println(u'    "%s": "%s", // %s [%s]' % (zone, target, alias, zone.filename))
        println(u"};")
        println(u"")

        println(u"// Legacy ICU time zones, these are not valid IANA time zone names. We also")
        println(u"// disallow the old and deprecated System V time zones.")
        println(u"// http://source.icu-project.org/repos/icu/icu/trunk/source/tools/tzcode/icuzones")
        println(u"var legacyICUTimeZones = {")
        for zone in sorted(legacylinks.keys()) + sorted(list(legacyzones)):
            println(u'    "%s": true,' % zone)
        println(u"};")

def updateTzdata(args):
    """ Update the IntlTzData.js file. """
    version = args.version
    if not re.match("^([0-9]{4}[a-z])$", version):
        raise RuntimeError("illegal version: %s" % version)
    url = args.url
    if url is None:
        url = "https://www.iana.org/time-zones/repository/releases/tzdata%s.tar.gz" % version
    tzDir = args.tz
    if tzDir is not None and not (os.path.isdir(tzDir) or os.path.isfile(tzDir)):
        raise RuntimeError("not a directory or file: %s" % tzDir)
    icuDir = args.icu
    if not os.path.isdir(icuDir):
        raise RuntimeError("not a directory: %s" % icuDir)
    icuTzDir = args.icutz
    if icuTzDir is None:
        icuTzDir = os.path.join(icuDir, "data/misc")
    if not os.path.isdir(icuTzDir):
        raise RuntimeError("not a directory: %s" % icuTzDir)
    out = args.out

    print("Arguments:")
    print("\ttzdata version: %s" % version)
    print("\ttzdata URL: %s" % url)
    print("\ttzdata directory|file: %s" % tzDir)
    print("\tICU directory: %s" % icuDir)
    print("\tICU timezone directory: %s" % icuTzDir)
    print("\tOutput file: %s" % out)
    print("")

    def updateFrom(f):
        if os.path.isfile(f) and tarfile.is_tarfile(f):
            with tarfile.open(f, "r:*") as tar:
                processTimeZones(TzDataFile(tar), icuDir, icuTzDir, version, out)
        elif os.path.isdir(f):
            processTimeZones(TzDataDir(f), icuDir, icuTzDir, version, out)
        else:
            raise RuntimError("unknown format")

    if tzDir is None:
        print("Downloading tzdata file...")
        with closing(urllib2.urlopen(url)) as tzfile:
            fname = urlparse.urlsplit(tzfile.geturl()).path.split('/')[-1]
            with tempfile.NamedTemporaryFile(suffix=fname) as tztmpfile:
                print("File stored in %s" % tztmpfile.name)
                tztmpfile.write(tzfile.read())
                tztmpfile.flush()
                updateFrom(tztmpfile.name)
    else:
        updateFrom(tzDir)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description="Update intl data.")
    subparsers = parser.add_subparsers(help="Select update mode")

    parser_tags = subparsers.add_parser("langtags", help="Update language-subtag-registry", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser_tags.add_argument("--url", metavar="URL", default="http://www.iana.org/assignments/language-subtag-registry", help="Download url for language-subtag-registry.txt")
    parser_tags.add_argument("--out", default="IntlData.js", help="Output file")
    parser_tags.add_argument("file", nargs="?", help="Local language-subtag-registry.txt file, if omitted uses <URL>")
    parser_tags.set_defaults(func=updateLangTags)

    parser_tz = subparsers.add_parser("tzdata", help="Update tzdata")
    parser_tz.add_argument("--icu", metavar="ICU", required=True, help="ICU source directory")
    parser_tz.add_argument("--icutz", help="ICU timezone directory, defaults to <ICU>/data/misc")
    parser_tz.add_argument("--tz", help="Local tzdata directory or file, if omitted uses <URL>")
    parser_tz.add_argument("--url", metavar="URL", help="Download url for tzdata, defaults to \"https://www.iana.org/time-zones/repository/releases/tzdata<VERSION>.tar.gz\"")
    parser_tz.add_argument("--version", metavar="VERSION", required=True, help="tzdata version")
    parser_tz.add_argument("--out", default="IntlTzData.js", help="Output file, defaults to \"IntlTzData.js\"")
    parser_tz.set_defaults(func=updateTzdata)

    args = parser.parse_args()
    args.func(args)
