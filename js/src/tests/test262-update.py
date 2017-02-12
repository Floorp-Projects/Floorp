#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import contextlib
import io
import os
import tempfile
import shutil
import sys

from functools import partial
from itertools import chain, imap

@contextlib.contextmanager
def TemporaryDirectory():
    tmpDir = tempfile.mkdtemp()
    try:
        yield tmpDir
    finally:
        shutil.rmtree(tmpDir)

def loadTest262Parser(test262Dir):
    """
    Loads the test262 test record parser.
    """
    import imp

    fileObj = None
    try:
        moduleName = "parseTestRecord"
        packagingDir = os.path.join(test262Dir, "tools", "packaging")
        (fileObj, pathName, description) = imp.find_module(moduleName, [packagingDir])
        return imp.load_module(moduleName, fileObj, pathName, description)
    finally:
        if fileObj:
            fileObj.close()

def tryParseTestFile(test262parser, source, testName):
    """
    Returns the result of test262parser.parseTestRecord() or None if a parser
    error occured.

    See <https://github.com/tc39/test262/blob/master/INTERPRETING.md> for an
    overview of the returned test attributes.
    """
    try:
        return test262parser.parseTestRecord(source, testName)
    except Exception as err:
        # TODO: Report erroneous files to test262 Github repository.
        # print("Error '%s' in file: %s" % (err, testName), file=sys.stderr)
        return None

def makeRefTestLine(refTest):
    """
    Creates the |reftest| entry from the input list. Or None if no reftest
    entry is required.
    """

    (refTestSkip, refTestSkipIf) = refTest

    if refTestSkip:
        comments = ", ".join(refTestSkip)
        return "skip -- %s" % comments

    if refTestSkipIf:
        conditions = "||".join([cond for (cond, _) in refTestSkipIf])
        comments = ", ".join([comment for (_, comment) in refTestSkipIf])
        return "skip-if(%s) -- %s" % (conditions, comments)

    return None

def createSource(testName, testSource, refTest, directive, epilogue):
    """
    Returns the post-processed source for |testSource|.
    """

    source = testSource

    # Prepend a possible "use strict" directive.
    if directive:
        source = directive + "\n" + source

    # Add the |reftest| if present.
    refTestLine = makeRefTestLine(refTest)
    if refTestLine:
        source = "// |reftest| " + refTestLine + "\n" + source

    # Append the test epilogue, i.e. the call to "reportCompare".
    # TODO: Does this conflict with raw tests?
    source += epilogue

    return source

def writeTestFile(test262OutDir, testFileName, source):
    """
    Writes the test source to |test262OutDir|.
    """

    with io.open(os.path.join(test262OutDir, testFileName), "wb") as output:
        output.write(source)

def addSuffixToFileName(fileName, suffix):
    (filePath, ext) = os.path.splitext(fileName)
    return filePath + suffix + ext

def writeShellAndBrowserFiles(test262OutDir, harnessDir, includesMap, relPath, helperFiles=[]):
    """
    Generate the shell.js and browser.js files for the test harness.
    """

    # Find all includes from parent directories.
    def findParentIncludes(relPath, includesMap):
        parentIncludes = set()
        current = relPath
        while current:
            (parent, child) = os.path.split(current)
            if parent in includesMap:
                parentIncludes.update(includesMap[parent])
            current = parent
        return parentIncludes

    # Find all includes, skipping includes already present in parent directories.
    def findIncludes(includesMap, relPath):
        includeSet = includesMap[relPath]
        parentIncludes = findParentIncludes(relPath, includesMap)
        for include in includeSet:
            if include not in parentIncludes:
                yield include

    def readIncludeFile(filePath):
        with io.open(filePath, "rb") as includeFile:
            return "// file: %s\n%s" % (os.path.basename(filePath), includeFile.read())

    # Concatenate all includes files.
    includeSource = "\n".join(imap(readIncludeFile, chain(
        # The requested include files.
        imap(partial(os.path.join, harnessDir), findIncludes(includesMap, relPath)),

        # And additional local helper files.
        imap(partial(os.path.join, os.getcwd()), helperFiles)
    )))

    # Write the concatenated include sources to shell.js.
    with io.open(os.path.join(test262OutDir, relPath, "shell.js"), "wb") as shellFile:
        shellFile.write(includeSource)

    # The browser.js file is always empty for test262 tests.
    with io.open(os.path.join(test262OutDir, relPath, "browser.js"), "wb") as browserFile:
        browserFile.write("")

def pathStartsWith(path, *args):
    prefix = os.path.join(*args)
    return os.path.commonprefix([path, prefix]) == prefix

def fileNameEndsWith(filePath, suffix):
    return os.path.splitext(os.path.basename(filePath))[0].endswith(suffix)

def convertTestFile(test262parser, testSource, testName, includeSet, strictTests):
    """
    Convert a test262 test to a compatible jstests test file.
    """

    # The test record dictionary, its contents are explained in depth at
    # <https://github.com/tc39/test262/blob/master/INTERPRETING.md>.
    testRec = tryParseTestFile(test262parser, testSource, testName)

    # jsreftest meta data
    refTestSkip = []
    refTestSkipIf = []
    refTest = (refTestSkip, refTestSkipIf)

    # Skip all files which contain YAML errors.
    if testRec is None:
        refTestSkip.append("has YAML errors")
        testRec = dict()

    # onlyStrict is set when the test must only be run in strict mode.
    onlyStrict = "onlyStrict" in testRec

    # noStrict is set when the test must not be run in strict mode.
    noStrict = "noStrict" in testRec

    # The "raw" attribute is used in the default test262 runner to prevent
    # prepending additional content (use-strict directive, harness files)
    # before the actual test source code. We can probably ignore it.
    raw = "raw" in testRec
    assert not (raw and (onlyStrict or noStrict))

    # Most async tests are marked with the "async" attribute. But fallback to
    # detect async tests when $DONE is present in the test source code.
    async = "async" in testRec or "$DONE" in testSource

    # Most negative tests use {type=<error name>, phase=early|runtime}, except
    # those which weren't update. These are still using negative=<error name>.
    if "negative" in testRec:
        negative = testRec["negative"]
        # errorType is currently ignored. :-(
        errorType = negative["type"] if type(negative) == dict else negative
        isNegative = True
    else:
        isNegative = False

    # Skip non-test files.
    isSupportFile = fileNameEndsWith(testName, "FIXTURE")
    if isSupportFile:
        refTestSkip.append("not a test file")

    # Skip all module tests.
    if "module" in testRec or pathStartsWith(testName, "language", "module-code"):
        refTestSkip.append("jstests don't yet support module tests")

    # Skip tests with unsupported features.
    if "features" in testRec:
        # tail-call-optimization isn't implemented in SpiderMonkey, skip all tests
        # marked with this feature.
        unsupportedFeatures = set(["tail-call-optimization"])

        unsupported = unsupportedFeatures.intersection(testRec["features"])
        if unsupported:
            refTestSkip.append("%s is not supported" % ",".join(list(unsupported)))

    # Includes for every test file in a directory is collected in a single
    # shell.js file per directory level. This is done to avoid adding all
    # test harness files to the top level shell.js file.
    if "includes" in testRec:
        assert not raw, "Raw test with includes: %s" % testName
        includeSet.update(testRec["includes"])

    # Skip intl402 tests when Intl isn't available.
    if pathStartsWith(testName, "intl402"):
        refTestSkipIf.append(("!this.hasOwnProperty('Intl')", "needs Intl"))

    # Skip built-ins/Simd tests when SIMD isn't available.
    if pathStartsWith(testName, "built-ins", "Simd"):
        refTestSkipIf.append(("!this.hasOwnProperty('SIMD')", "needs SIMD"))

    # Add reportCompare() after all positive, synchronous tests.
    if not isNegative and not async:
        testEpilogue = """
reportCompare(0, 0);
"""
    else:
        testEpilogue = ""

    # Write raw or non-strict mode test.
    if raw or noStrict or not onlyStrict:
        nonStrictSource = createSource(testName, testSource, refTest, "", testEpilogue)
        testFileName = testName
        if isNegative:
            testFileName = addSuffixToFileName(testFileName, "-n")
        yield (testFileName, nonStrictSource)

    # Write strict mode test.
    if not raw and not isSupportFile and (onlyStrict or (not noStrict and strictTests)):
        strictSource = createSource(testName, testSource, refTest, "'use strict';", testEpilogue)
        testFileName = testName
        if not noStrict:
            testFileName = addSuffixToFileName(testFileName, "-strict")
        if isNegative:
            testFileName = addSuffixToFileName(testFileName, "-n")
        yield (testFileName, strictSource)

def process_test262(test262Dir, test262OutDir, strictTests):
    """
    Process all test262 files and converts them into jstests compatible tests.
    """

    harnessDir = os.path.join(test262Dir, "harness")
    testDir = os.path.join(test262Dir, "test")
    test262parser = loadTest262Parser(test262Dir)

    # map<dirname, set<includeFiles>>
    includesMap = {}

    # Root directory contains required harness files.
    includesMap[""] = set(["sta.js", "assert.js"])

    # Also add files known to be used by many tests to the root shell.js file.
    includesMap[""].update(["propertyHelper.js", "compareArray.js"])

    # Add "test262-host.js" to the root shell.js file.
    writeShellAndBrowserFiles(test262OutDir, harnessDir, includesMap, "", ["test262-host.js"])

    # Additional explicit includes inserted at well-chosen locations to reduce
    # code duplication in shell.js files.
    explicitIncludes = {}
    explicitIncludes["intl402"] = ["testBuiltInObject.js"]
    explicitIncludes[os.path.join("built-ins", "DataView")] = ["byteConversionValues.js"]
    explicitIncludes[os.path.join("built-ins", "Promise")] = ["PromiseHelper.js"]
    explicitIncludes[os.path.join("built-ins", "TypedArray")] = ["byteConversionValues.js",
        "detachArrayBuffer.js", "nans.js"]
    explicitIncludes[os.path.join("built-ins", "TypedArrays")] = ["detachArrayBuffer.js"]

    # Process all test directories recursively.
    for (dirPath, dirNames, fileNames) in os.walk(testDir):
        relPath = os.path.relpath(dirPath, testDir)
        if relPath == ".":
            continue
        os.makedirs(os.path.join(test262OutDir, relPath))

        includeSet = set()
        includesMap[relPath] = includeSet

        if relPath in explicitIncludes:
            includeSet.update(explicitIncludes[relPath])

        # Convert each test file.
        for fileName in fileNames:
            filePath = os.path.join(dirPath, fileName)
            testName = os.path.relpath(filePath, testDir)

            # Read the original test source and preprocess it for the jstests harness.
            with io.open(filePath, "rb") as testFile:
                testSource = testFile.read()

            for (newFileName, newSource) in convertTestFile(test262parser, testSource, testName, includeSet, strictTests):
                writeTestFile(test262OutDir, newFileName, newSource)

        # Add shell.js and browers.js files for the current directory.
        writeShellAndBrowserFiles(test262OutDir, harnessDir, includesMap, relPath)

def update_test262(args):
    import subprocess

    url = args.url
    branch = args.branch
    revision = args.revision
    outDir = args.out
    if not os.path.isabs(outDir):
        outDir = os.path.join(os.getcwd(), outDir)
    strictTests = args.strict

    # Download the requested branch in a temporary directory.
    with TemporaryDirectory() as inDir:
        if revision == "HEAD":
            subprocess.check_call(["git", "clone", "--depth=1", "--branch=%s" % branch, url, inDir])
        else:
            subprocess.check_call(["git", "clone", "--single-branch", "--branch=%s" % branch, url, inDir])
            subprocess.check_call(["git", "-C", inDir, "reset", "--hard", revision])

        # Create the output directory from scratch.
        if os.path.isdir(outDir):
            shutil.rmtree(outDir)
        os.makedirs(outDir)

        # Copy license file.
        shutil.copyfile(os.path.join(inDir, "LICENSE"), os.path.join(outDir, "LICENSE"))

        # Create the git info file.
        with io.open(os.path.join(outDir, "GIT-INFO"), "wb") as info:
            subprocess.check_call(["git", "-C", inDir, "log", "-1"], stdout=info)

        # Copy the test files.
        process_test262(inDir, outDir, strictTests)

if __name__ == "__main__":
    import argparse

    # This script must be run from js/src/tests to work correctly.
    if "/".join(os.path.normpath(os.getcwd()).split(os.sep)[-3:]) != "js/src/tests":
        raise RuntimeError("%s must be run from js/src/tests" % sys.argv[0])

    parser = argparse.ArgumentParser(description="Update the test262 test suite.")
    parser.add_argument("--url", default="git://github.com/tc39/test262.git",
                        help="URL to git repository (default: %(default)s)")
    parser.add_argument("--branch", default="master",
                        help="Git branch (default: %(default)s)")
    parser.add_argument("--revision", default="HEAD",
                        help="Git revision (default: %(default)s)")
    parser.add_argument("--out", default="test262",
                        help="Output directory. Any existing directory will be removed! (default: %(default)s)")
    parser.add_argument("--strict", default=False, action="store_true",
                        help="Generate additional strict mode tests. Not enabled by default.")
    parser.set_defaults(func=update_test262)
    args = parser.parse_args()
    args.func(args)
