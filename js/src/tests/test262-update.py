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

# Skip all tests which use features not supported in SpiderMonkey.
UNSUPPORTED_FEATURES = set(["tail-call-optimization"])
RELEASE_OR_BETA = set(["async-iteration"])

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
        print("Error '%s' in file: %s" % (err, testName), file=sys.stderr)
        print("Please report this error to the test262 GitHub repository!")
        return None

def createRefTestEntry(skip, skipIf, error, isModule):
    """
    Creates the |reftest| entry from the input list. Or the empty string if no
    reftest entry is required.
    """

    terms = []
    comments = []

    if skip:
        terms.append("skip")
        comments.extend(skip)

    if skipIf:
        terms.append("skip-if(" + "||".join([cond for (cond, _) in skipIf]) + ")")
        comments.extend([comment for (_, comment) in skipIf])

    if error:
        terms.append("error:" + error)

    if isModule:
        terms.append("module")

    line = " ".join(terms)
    if comments:
        line += " -- " + ", ".join(comments)

    return line

def createSource(testSource, refTest, prologue, epilogue):
    """
    Returns the post-processed source for |testSource|.
    """

    source = testSource

    # Prepend any directives if present.
    if prologue:
        source = prologue + "\n" + source

    # Add the |reftest| line.
    if refTest:
        source = "// |reftest| " + refTest + "\n" + source

    # Append the test epilogue, i.e. the call to "reportCompare".
    # TODO: Does this conflict with raw tests?
    if epilogue:
        source += "\n" + epilogue + "\n"

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

def writeShellAndBrowserFiles(test262OutDir, harnessDir, includesMap, localIncludesMap, relPath):
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

    localIncludes = localIncludesMap[relPath] if relPath in localIncludesMap else []

    # Concatenate all includes files.
    includeSource = "\n".join(imap(readIncludeFile, chain(
        # The requested include files.
        imap(partial(os.path.join, harnessDir), findIncludes(includesMap, relPath)),

        # And additional local include files.
        imap(partial(os.path.join, os.getcwd()), localIncludes)
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
    # before the actual test source code.
    raw = "raw" in testRec

    # Async tests are marked with the "async" attribute. It is an error for a
    # test to use the $DONE function without specifying the "async" attribute.
    async = "async" in testRec
    assert "$DONE" not in testSource or async, "Missing async attribute in: %s" % testName

    # When the "module" attribute is set, the source code is module code.
    isModule = "module" in testRec

    # Negative tests have additional meta-data to specify the error type and
    # when the error is issued (runtime error or early parse error). We're
    # currently ignoring the error phase attribute.
    # testRec["negative"] == {type=<error name>, phase=early|runtime}
    isNegative = "negative" in testRec
    assert not isNegative or type(testRec["negative"]) == dict
    errorType = testRec["negative"]["type"] if isNegative else None

    # Skip non-test files.
    isSupportFile = fileNameEndsWith(testName, "FIXTURE")
    if isSupportFile:
        refTestSkip.append("not a test file")

    # Skip tests with unsupported features.
    if "features" in testRec:
        unsupported = UNSUPPORTED_FEATURES.intersection(testRec["features"])
        releaseOrBeta = RELEASE_OR_BETA.intersection(testRec["features"])
        if unsupported:
            refTestSkip.append(
              "%s is not supported" % ",".join(list(unsupported)))
        elif releaseOrBeta:
            refTestSkipIf.append(
              ("release_or_beta",
               "%s is not released yet" % ",".join(list(releaseOrBeta)))
            )
        elif "SharedArrayBuffer" in testRec["features"]:
            refTestSkipIf.append(
              ("!this.hasOwnProperty('SharedArrayBuffer')",
               "SharedArrayBuffer not yet riding the trains")
            )

    # Includes for every test file in a directory is collected in a single
    # shell.js file per directory level. This is done to avoid adding all
    # test harness files to the top level shell.js file.
    if "includes" in testRec:
        assert not raw, "Raw test with includes: %s" % testName
        includeSet.update(testRec["includes"])

    # Add reportCompare() after all positive, synchronous tests.
    if not isNegative and not async and not isSupportFile:
        testEpilogue = "reportCompare(0, 0);"
    else:
        testEpilogue = ""

    refTest = createRefTestEntry(refTestSkip, refTestSkipIf, errorType, isModule)

    # Don't write a strict-mode variant for raw, module or support files.
    noStrictVariant = raw or isModule or isSupportFile
    assert not (noStrictVariant and (onlyStrict or noStrict)),\
           "Unexpected onlyStrict or noStrict attribute: %s" % testName

    # Write non-strict mode test.
    if noStrictVariant or noStrict or not onlyStrict:
        testPrologue = ""
        nonStrictSource = createSource(testSource, refTest, testPrologue, testEpilogue)
        testFileName = testName
        yield (testFileName, nonStrictSource)

    # Write strict mode test.
    if not noStrictVariant and (onlyStrict or (not noStrict and strictTests)):
        testPrologue = "'use strict';"
        strictSource = createSource(testSource, refTest, testPrologue, testEpilogue)
        testFileName = testName
        if not noStrict:
            testFileName = addSuffixToFileName(testFileName, "-strict")
        yield (testFileName, strictSource)

def process_test262(test262Dir, test262OutDir, strictTests):
    """
    Process all test262 files and converts them into jstests compatible tests.
    """

    harnessDir = os.path.join(test262Dir, "harness")
    testDir = os.path.join(test262Dir, "test")
    test262parser = loadTest262Parser(test262Dir)

    # Map of test262 subdirectories to the set of include files required for
    # tests in that subdirectory. The includes for all tests in a subdirectory
    # are merged into a single shell.js.
    # map<dirname, set<includeFiles>>
    includesMap = {}

    # Additional local includes keyed by test262 directory names. The include
    # files in this map must be located in the js/src/tests directory.
    # map<dirname, list<includeFiles>>
    localIncludesMap = {}

    # The root directory contains required harness files and test262-host.js.
    includesMap[""] = set(["sta.js", "assert.js"])
    localIncludesMap[""] = ["test262-host.js"]

    # Also add files known to be used by many tests to the root shell.js file.
    includesMap[""].update(["propertyHelper.js", "compareArray.js"])

    # Write the root shell.js file.
    writeShellAndBrowserFiles(test262OutDir, harnessDir, includesMap, localIncludesMap, "")

    # Additional explicit includes inserted at well-chosen locations to reduce
    # code duplication in shell.js files.
    explicitIncludes = {}
    explicitIncludes["intl402"] = ["testBuiltInObject.js"]
    explicitIncludes[os.path.join("built-ins", "DataView")] = ["byteConversionValues.js"]
    explicitIncludes[os.path.join("built-ins", "Promise")] = ["PromiseHelper.js"]
    explicitIncludes[os.path.join("built-ins", "TypedArray")] = ["byteConversionValues.js",
        "detachArrayBuffer.js", "nans.js"]
    explicitIncludes[os.path.join("built-ins", "TypedArrays")] = ["detachArrayBuffer.js"]

    # Intl.PluralRules isn't yet enabled by default.
    localIncludesMap[os.path.join("intl402", "PluralRules")] = ["test262-intl-extras.js"]

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

            # Copy non-test files as is.
            (_, fileExt) = os.path.splitext(fileName)
            if fileExt != ".js":
                shutil.copyfile(filePath, os.path.join(test262OutDir, testName))
                continue

            # Read the original test source and preprocess it for the jstests harness.
            with io.open(filePath, "rb") as testFile:
                testSource = testFile.read()

            for (newFileName, newSource) in convertTestFile(test262parser, testSource, testName, includeSet, strictTests):
                writeTestFile(test262OutDir, newFileName, newSource)

        # Add shell.js and browers.js files for the current directory.
        writeShellAndBrowserFiles(test262OutDir, harnessDir, includesMap, localIncludesMap, relPath)

def update_test262(args):
    import subprocess

    url = args.url
    branch = args.branch
    revision = args.revision
    outDir = args.out
    if not os.path.isabs(outDir):
        outDir = os.path.join(os.getcwd(), outDir)
    strictTests = args.strict
    localTestsOutDir = os.path.join(outDir, "local");

    # Download the requested branch in a temporary directory.
    with TemporaryDirectory() as inDir:
        if revision == "HEAD":
            subprocess.check_call(["git", "clone", "--depth=1", "--branch=%s" % branch, url, inDir])
        else:
            subprocess.check_call(["git", "clone", "--single-branch", "--branch=%s" % branch, url, inDir])
            subprocess.check_call(["git", "-C", inDir, "reset", "--hard", revision])

        # Stash test262/local. Currently the test262 repo does not have any
        # top-level subdirectory named "local".
        restoreLocalTestsDir = False
        if os.path.isdir(localTestsOutDir):
            shutil.move(localTestsOutDir, inDir)
            restoreLocalTestsDir = True

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

        # Move test262/local back.
        if restoreLocalTestsDir:
            shutil.move(os.path.join(inDir, "local"), outDir)

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
