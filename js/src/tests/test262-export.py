#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import contextlib
import os
import re
import tempfile
import shutil
import sys
import yaml

from functools import partial
from itertools import chain, imap

# Skip all common files used to support tests for jstests
# These files are listed in the README.txt
SUPPORT_FILES = set(["browser.js", "shell.js", "template.js", "user.js",
    "js-test-driver-begin.js", "js-test-driver-end.js"])

FRONTMATTER_WRAPPER_PATTERN = re.compile(
    r'/\*\---\n([\s]*)((?:\s|\S)*)[\n\s*]---\*/', flags=re.DOTALL)

def convertTestFile(source, includes):
    """
    Convert a jstest test to a compatible Test262 test file.
    """

    source = convertReportCompare(source)
    source = updateMeta(source, includes)
    source = insertCopyrightLines(source)

    return source

def convertReportCompare(source):
    """
    Captures all the reportCompare and convert them accordingly.

    Cases with reportCompare calls where the arguments are the same and one of
    0, true, or null, will be discarded as they are not necessary for Test262.

    Otherwise, reportCompare will be replaced with assert.sameValue, as the
    equivalent in Test262
    """

    def replaceFn(matchobj):
        actual = matchobj.group(1)
        expected = matchobj.group(2)

        if actual == expected and actual in ["0", "true", "null"]:
            return ""

        return matchobj.group()

    newSource = re.sub(
        r'.*reportCompare\s*\(\s*(\w*)\s*,\s*(\w*)\s*(,\s*\S*)?\s*\)\s*;*\s*',
        replaceFn,
        source
    )

    return re.sub(r'\breportCompare\b', "assert.sameValue", newSource)

def fetchReftestEntries(reftest):
    """
    Collects and stores the entries from the reftest header.
    """

    # TODO: fails, slow, skip, random, random-if

    features = []
    error = None
    comments = None
    module = False

    # should capture conditions to skip
    matchesSkip = re.search(r'skip-if\((.*)\)', reftest)
    if matchesSkip:
        matches = matchesSkip.group(1).split("||")
        for match in matches:
            # captures a features list
            dependsOnProp = re.search(
                r'!this.hasOwnProperty\([\'\"](.*?)[\'\"]\)', match)
            if dependsOnProp:
                features.append(dependsOnProp.group(1))
            else:
                print("# Can't parse the following skip-if rule: %s" % match)

    # should capture the expected error
    matchesError = re.search(r'error:\s*(\w*)', reftest)
    if matchesError:
        # The metadata from the reftests won't say if it's a runtime or an
        # early error. This specification is required for the frontmatter tags.
        error = matchesError.group(1)

    # just tells if it's a module
    matchesModule = re.search(r'\bmodule\b', reftest)
    if matchesModule:
        module = True

    # captures any comments
    matchesComments = re.search(r' -- (.*)', reftest)
    if matchesComments:
        comments = matchesComments.group(1)

    return {
        "features": features,
        "error": error,
        "module": module,
        "info": comments
    }

def parseHeader(source):
    """
    Parse the source to return it with the extracted the header
    """
    from lib.manifest import TEST_HEADER_PATTERN_INLINE

    # Bail early if we do not start with a single comment.
    if not source.startswith("//"):
        return (source, {})

    # Extract the token.
    part, _, _ = source.partition("\n")
    matches = TEST_HEADER_PATTERN_INLINE.match(part)

    if matches and matches.group(0):
        reftest = matches.group(0)

        # Remove the found header from the source;
        # Fetch and return the reftest entries
        return (source.replace(reftest + "\n", ""), fetchReftestEntries(reftest))

    return (source, {})

def extractMeta(source):
    """
    Capture the frontmatter metadata as yaml if it exists.
    Returns a new dict if it doesn't.
    """

    match = FRONTMATTER_WRAPPER_PATTERN.search(source)
    if not match:
        return {}

    indent, frontmatter_lines = match.groups()

    unindented = re.sub('^%s' % indent, '', frontmatter_lines)

    return yaml.safe_load(unindented)

def updateMeta(source, includes):
    """
    Captures the reftest meta and a pre-existing meta if any and merge them
    into a single dict.
    """

    # Extract the reftest data from the source
    source, reftest = parseHeader(source)

    # Extract the frontmatter data from the source
    frontmatter = extractMeta(source)

    # Merge the reftest and frontmatter
    merged = mergeMeta(reftest, frontmatter, includes)

    # Cleanup the metadata
    properData = cleanupMeta(merged)

    return insertMeta(source, properData)


def cleanupMeta(meta):
    """
    Clean up all the frontmatter meta tags. This is not a lint tool, just a
    simple cleanup to remove trailing spaces and duplicate entries from lists.
    """

    # Populate required tags
    for tag in ("description", "esid"):
        meta.setdefault(tag, "pending")

    # Trim values on each string tag
    for tag in ("description", "esid", "es5id", "es6id", "info", "author"):
        if tag in meta:
            meta[tag] = meta[tag].strip()

    # Remove duplicate entries on each list tag
    for tag in ("features", "flags", "includes"):
        if tag in meta:
            # We need the list back for the yaml dump
            meta[tag] = list(set(meta[tag]))

    if "negative" in meta:
        # If the negative tag exists, phase needs to be present and set
        if meta["negative"].get("phase") not in ("early", "runtime"):
            print("Warning: the negative.phase is not properly set.\n" + \
                "Ref https://github.com/tc39/test262/blob/master/INTERPRETING.md#negative")
        # If the negative tag exists, type is required
        if "type" not in meta["negative"]:
            print("Warning: the negative.type is not set.\n" + \
                "Ref https://github.com/tc39/test262/blob/master/INTERPRETING.md#negative")

    return meta

def mergeMeta(reftest, frontmatter, includes):
    """
    Merge the metadata from reftest and an existing frontmatter and populate
    required frontmatter fields properly.
    """

    # Merge the meta from reftest to the frontmatter

    if "features" in reftest:
        frontmatter.setdefault("features", []) \
            .extend(reftest.get("features", []))

    # Only add the module flag if the value from reftest is truish
    if reftest.get("module"):
        frontmatter.setdefault("flags", []).append("module")

    # Add any comments to the info tag
    info = reftest.get("info")
    if info:
        # Open some space in an existing info text
        if "info" in frontmatter:
            frontmatter["info"] += "\n\n  \%" % info
        else:
            frontmatter["info"] = info

    # Set the negative flags
    if "error" in reftest:
        error = reftest["error"]
        if "negative" not in frontmatter:
            frontmatter["negative"] = {
                # This code is assuming error tags are early errors, but they
                # might be runtime errors as well.
                # From this point, this code can also print a warning asking to
                # specify the error phase in the generated code or fill the
                # phase with an empty string.
                "phase": "early",
                "type": error
            }
        # Print a warning if the errors don't match
        elif frontmatter["negative"].get("type") != error:
            print("Warning: The reftest error doesn't match the existing " + \
                "frontmatter error. %s != %s" % (error,
                frontmatter["negative"]["type"]))

    # Add the shell specific includes
    if includes:
        frontmatter["includes"] = list(includes)

    return frontmatter

def insertCopyrightLines(source):
    """
    Insert the copyright lines into the file.
    """
    from datetime import date

    lines = []

    if not re.match(r'\/\/\s+Copyright.*\. All rights reserved.', source):
        year = date.today().year
        lines.append("// Copyright (C) %s Mozilla Corporation. All rights reserved." % year)
        lines.append("// This code is governed by the BSD license found in the LICENSE file.")
        lines.append("\n")

    return "\n".join(lines) + source

def insertMeta(source, frontmatter):
    """
    Insert the formatted frontmatter into the file, use the current existing
    space if any
    """
    lines = []

    lines.append("/*---")

    for (key, value) in frontmatter.items():
        if key in ("description", "info"):
            lines.append("%s: |" % key)
            lines.append("  " + yaml.dump(value, encoding="utf8",
                ).strip().replace('\n...', ''))
        else:
            lines.append(yaml.dump({key: value}, encoding="utf8",
                default_flow_style=False).strip())

    lines.append("---*/")

    match = FRONTMATTER_WRAPPER_PATTERN.search(source)

    if match:
        return source.replace(match.group(0), "\n".join(lines))
    else:
        return "\n".join(lines) + source



def findAndCopyIncludes(dirPath, baseDir, includeDir):
    relPath = os.path.relpath(dirPath, baseDir)
    includes = []

    # Recurse down all folders in the relative path until
    # we reach the base directory of shell.js include files.
    # Each directory will have a shell.js file to copy.
    while (relPath):

        # find the shell.js
        shellFile = os.path.join(baseDir, relPath, "shell.js")

        # create new shell.js file name
        includeFileName = relPath.replace("/", "-") + "-shell.js"
        includesPath = os.path.join(includeDir, includeFileName)

        if os.path.exists(shellFile):
            # if the file exists, include in includes
            includes.append(includeFileName)

            if not os.path.exists(includesPath):
                shutil.copyfile(shellFile, includesPath)

        relPath = os.path.split(relPath)[0]


    shellFile = os.path.join(baseDir, "shell.js")
    includesPath = os.path.join(includeDir, "shell.js")
    if not os.path.exists(includesPath):
        shutil.copyfile(shellFile, includesPath)

    includes.append("shell.js")

    if not os.path.exists(includesPath):
        shutil.copyfile(shellFile, includesPath)

    return includes

def exportTest262(args):

    outDir = os.path.abspath(args.out)
    providedSrcs = args.src
    includeShell = args.exportshellincludes
    baseDir = os.getcwd()

    # Create the output directory from scratch.
    if os.path.isdir(outDir):
        shutil.rmtree(outDir)

    # only make the includes directory if requested
    includeDir = os.path.join(outDir, "harness-includes")
    if includeShell:
        os.makedirs(includeDir)

    # Go through each source path
    for providedSrc in providedSrcs:

        src = os.path.abspath(providedSrc)
        # the basename of the path will be used in case multiple "src" arguments
        # are passed in to create an output directory for each "src".
        basename = os.path.basename(src)

        # Process all test directories recursively.
        for (dirPath, _, fileNames) in os.walk(src):

            # we need to make and get the unique set of includes for this filepath
            includes = []
            if includeShell:
                includes = findAndCopyIncludes(dirPath, baseDir, includeDir)

            relPath = os.path.relpath(dirPath, src)
            fullRelPath = os.path.join(basename, relPath)

            # Make new test subdirectory to seperate from includes
            currentOutDir = os.path.join(outDir, "tests", fullRelPath)

            # This also creates the own outDir folder
            if not os.path.exists(currentOutDir):
                os.makedirs(currentOutDir)

            for fileName in fileNames:
                # Skip browser.js files
                if fileName == "browser.js" or fileName == "shell.js" :
                    continue

                filePath = os.path.join(dirPath, fileName)
                testName = os.path.join(fullRelPath, fileName) # captures folder(s)+filename

                # Copy non-test files as is.
                (_, fileExt) = os.path.splitext(fileName)
                if fileExt != ".js":
                    shutil.copyfile(filePath, os.path.join(currentOutDir, fileName))
                    print("C %s" % testName)
                    continue

                # Read the original test source and preprocess it for Test262
                with open(filePath, "rb") as testFile:
                    testSource = testFile.read()

                if not testSource:
                    print("SKIPPED %s" % testName)
                    continue

                newSource = convertTestFile(testSource, includes)

                with open(os.path.join(currentOutDir, fileName), "wb") as output:
                    output.write(newSource)

                print("SAVED %s" % testName)

if __name__ == "__main__":
    import argparse

    # This script must be run from js/src/tests to work correctly.
    if "/".join(os.path.normpath(os.getcwd()).split(os.sep)[-3:]) != "js/src/tests":
        raise RuntimeError("%s must be run from js/src/tests" % sys.argv[0])

    parser = argparse.ArgumentParser(description="Export tests to match Test262 file compliance.")
    parser.add_argument("--out", default="test262/export",
                        help="Output directory. Any existing directory will be removed! (default: %(default)s)")
    parser.add_argument("--exportshellincludes", action="store_true",
                         help="Optionally export shell.js files as includes in exported tests. Only use for testing, do not use for exporting to test262 (test262 tests should have as few dependencies as possible).")
    parser.add_argument("src", nargs="+", help="Source folder with test files to export")
    parser.set_defaults(func=exportTest262)
    args = parser.parse_args()
    args.func(args)
