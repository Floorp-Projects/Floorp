#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from optparse import OptionParser
from subprocess import Popen, PIPE
import xml.dom.minidom
import html5lib
import filecmp
import fnmatch
import shutil
import sys
import re

# FIXME:
#   * Import more tests rather than just the very limited set currently
#     chosen.
#   * Read in a (checked-in) input file with a list of test assertions
#     expected to fail.
#   * Read in a (checked-in) input file with a list of reference choices
#     for tests with multiple rel="match" references.  (But still go
#     though all those references in case they, in turn, have references.)

# Eventually we should import all the tests that have references.  (At
# least for a subset of secs.  And we probably want to organize the
# directory structure by spec to avoid constant file moves when files
# move in the W3C repository.  And we probably also want to import each
# test only once, even if it covers more than one spec.)

# But for now, let's just import a few sets of tests.

gSubtrees = [
    os.path.join("css-namespaces-3"),
    os.path.join("css-conditional-3"),
    os.path.join("css-values-3"),
    os.path.join("selectors-4"),
]

gPrefixedProperties = [
    "column-count",
    "column-fill",
    "column-gap",
    "column-rule",
    "column-rule-color",
    "column-rule-style",
    "column-rule-width",
    "columns",
    "column-span",
    "column-width"
]

# Map of about:config prefs that need toggling, for a given test subdirectory.
# Entries should look like:
#  "$SUBDIR_NAME": "pref($PREF_NAME, $PREF_VALUE)"
#
# For example, when "@supports" was behind a pref, gDefaultPreferences had:
#  "css3-conditional": "pref(layout.css.supports-rule.enabled,true)"
gDefaultPreferences = {
}

gLog = None
gFailList = []
gDestPath = None
gSrcPath = None
support_dirs_mapped = set()
filemap = {}
speclinkmap = {}
propsaddedfor = []
tests = []
gOptions = None
gArgs = None
gTestfiles = []
gTestFlags = {}

def to_unix_path_sep(path):
    return path.replace('\\', '/')

def log_output_of(subprocess):
    global gLog
    subprocess.wait()
    if (subprocess.returncode != 0):
        raise StandardError("error while running subprocess")
    gLog.write(subprocess.stdout.readline().rstrip())

def write_log_header():
    global gLog, gSrcPath
    gLog.write("Importing revision: ")
    log_output_of(Popen(["hg", "parent", "--template={node}"],
                  stdout=PIPE, cwd=gSrcPath))
    gLog.write("\nfrom repository: ")
    log_output_of(Popen(["hg", "paths", "default"],
                  stdout=PIPE, cwd=gSrcPath))
    gLog.write("\n")

def remove_existing_dirs():
    global gDestPath
    # Remove existing directories that we're going to regenerate.  This
    # is necessary so that we can give errors in cases where our import
    # might copy two files to the same location, which we do by giving
    # errors if a copy would overwrite a file.
    for dirname in os.listdir(gDestPath):
        fulldir = os.path.join(gDestPath, dirname)
        if not os.path.isdir(fulldir):
            continue
        shutil.rmtree(fulldir)

def populate_test_files():
    global gSubtrees, gTestfiles
    excludeDirs = ["support", "reftest", "reference", "reports", "tools"]
    for subtree in gSubtrees:
        for dirpath, dirnames, filenames in os.walk(subtree, topdown=True):
            for exclDir in excludeDirs:
                if exclDir in dirnames:
                    dirnames.remove(exclDir)
            for f in filenames:
                if f == "README" or \
                   f.find("-ref.") != -1:
                    continue
                gTestfiles.append(os.path.join(dirpath, f))

    gTestfiles.sort()

def copy_file(test, srcfile, destname, isSupportFile=False):
    global gDestPath, gLog, gSrcPath
    if not srcfile.startswith(gSrcPath):
        raise StandardError("Filename " + srcfile + " does not start with " + gSrcPath)
    logname = srcfile[len(gSrcPath):]
    gLog.write("Importing " + to_unix_path_sep(logname) +
               " to " + to_unix_path_sep(destname) + "\n")
    destfile = os.path.join(gDestPath, destname)
    destdir = os.path.dirname(destfile)
    if not os.path.exists(destdir):
        os.makedirs(destdir)
    if os.path.exists(destfile):
        if filecmp.cmp(srcfile, destfile):
            print "Warning: duplicate file {}".format(destname)
            return
        raise StandardError("file " + destfile + " already exists")
    copy_and_prefix(test, srcfile, destfile, gPrefixedProperties, isSupportFile)

def copy_support_files(test, dirname, spec):
    if dirname in support_dirs_mapped:
        return
    support_dirs_mapped.add(dirname)
    support_dir = os.path.join(dirname, "support")
    if not os.path.exists(support_dir):
        return
    for dirpath, dirnames, filenames in os.walk(support_dir):
        for fn in filenames:
            if fn == "LOCK":
                continue
            full_fn = os.path.join(dirpath, fn)
            copy_file(test, full_fn, os.path.join(spec, "support", full_fn[len(support_dir)+1:]), True)

def map_file(fn, spec):
    if fn in filemap:
        return filemap[fn]
    destname = os.path.join(spec, os.path.basename(fn))
    filemap[fn] = destname
    load_flags_for(fn, spec)
    copy_file(destname, fn, destname, False)
    copy_support_files(destname, os.path.dirname(fn), spec)
    return destname

def load_flags_for(fn, spec):
    global gTestFlags
    document = get_document_for(fn)
    destname = os.path.join(spec, os.path.basename(fn))
    gTestFlags[destname] = []

    for meta in document.getElementsByTagName("meta"):
        name = meta.getAttribute("name")
        if name == "flags":
            gTestFlags[destname] = meta.getAttribute("content").split()

def is_html(fn):
    return fn.endswith(".htm") or fn.endswith(".html")

def get_document_for(fn):
    document = None # an xml.dom.minidom document
    if is_html(fn):
        # An HTML file
        f = open(fn, "rb")
        parser = html5lib.HTMLParser(tree=html5lib.treebuilders.getTreeBuilder("dom"))
        document = parser.parse(f)
        f.close()
    else:
        # An XML file
        document = xml.dom.minidom.parse(fn)
    return document

def add_test_items(fn, spec):
    document = get_document_for(fn)
    refs = []
    notrefs = []
    for link in document.getElementsByTagName("link"):
        rel = link.getAttribute("rel")
        if rel == "match":
            arr = refs
        elif rel == "mismatch":
            arr = notrefs
        else:
            continue
        arr.append(os.path.join(os.path.dirname(fn), str(link.getAttribute("href"))))
    if len(refs) > 1:
        raise StandardError("Need to add code to specify which reference we want to match.")
    if spec is None:
        for subtree in gSubtrees:
            if fn.startswith(subtree):
                spec = os.path.basename(subtree)
                break
        else:
            raise StandardError("Could not associate test " + fn + " with specification")
    for ref in refs:
        tests.append(["==", map_file(fn, spec), map_file(ref, spec)])
    for notref in notrefs:
        tests.append(["!=", map_file(fn, spec), map_file(notref, spec)])
    # Add chained references too
    for ref in refs:
        add_test_items(ref, spec=spec)
    for notref in notrefs:
        add_test_items(notref, spec=spec)

AHEM_DECL_CONTENT = """@font-face {
  font-family: Ahem;
  src: url("../../../fonts/Ahem.ttf");
}"""
AHEM_DECL_HTML = """<style type="text/css">
""" + AHEM_DECL_CONTENT + """
</style>
"""
AHEM_DECL_XML = """<style type="text/css"><![CDATA[
""" + AHEM_DECL_CONTENT + """
]]></style>
"""

def copy_and_prefix(test, aSourceFileName, aDestFileName, aProps, isSupportFile=False):
    global gTestFlags
    newFile = open(aDestFileName, 'wb')
    unPrefixedFile = open(aSourceFileName, 'rb')
    testName = aDestFileName[len(gDestPath)+1:]
    ahemFontAdded = False
    for line in unPrefixedFile:
        replacementLine = line
        searchRegex = "\s*<style\s*"

        if not isSupportFile and not ahemFontAdded and 'ahem' in gTestFlags[test] and re.search(searchRegex, line):
            # First put our ahem font declation before the first <style>
            # element
            newFile.write(AHEM_DECL_HTML if is_html(aDestFileName) else AHEM_DECL_XML)
            ahemFontAdded = True

        for prop in aProps:
            replacementLine = re.sub(r"([^-#]|^)" + prop + r"\b", r"\1-moz-" + prop, replacementLine)

        newFile.write(replacementLine)

    newFile.close()
    unPrefixedFile.close()

def read_options():
    global gArgs, gOptions
    op = OptionParser()
    op.usage = \
    '''%prog <clone of hg repository>
            Import reftests from a W3C hg repository clone. The location of
            the local clone of the hg repository must be given on the command
            line.'''
    (gOptions, gArgs) = op.parse_args()
    if len(gArgs) != 1:
        op.error("Too few arguments specified.")

def setup_paths():
    global gSubtrees, gDestPath, gSrcPath
    # FIXME: generate gSrcPath with consistent trailing / regardless of input.
    # (We currently expect the argument to have a trailing slash.)
    gSrcPath = gArgs[0]
    if not os.path.isdir(gSrcPath) or \
    not os.path.isdir(os.path.join(gSrcPath, ".hg")):
        raise StandardError("source path does not appear to be a mercurial clone")

    gDestPath = os.path.join(os.path.dirname(os.path.realpath(__file__)), "received")
    newSubtrees = []
    for relPath in gSubtrees:
        newSubtrees[len(gSubtrees):] = [os.path.join(gSrcPath, relPath)]
    gSubtrees = newSubtrees

def setup_log():
    global gLog
    # Since we're going to commit the tests, we should also commit
    # information about where they came from.
    gLog = open(os.path.join(gDestPath, "import.log"), "wb")

def read_fail_list():
    global gFailList
    dirname = os.path.realpath(__file__).split(os.path.sep)
    dirname = os.path.sep.join(dirname[:len(dirname)-1])
    with open(os.path.join(dirname, "failures.list"), "rb") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            items = line.split()
            pat = re.compile(fnmatch.translate(items.pop()))
            gFailList.append((pat, items))

def main():
    global gDestPath, gLog, gTestfiles, gTestFlags, gFailList
    read_options()
    setup_paths()
    read_fail_list()
    setup_log()
    write_log_header()
    remove_existing_dirs()
    populate_test_files()

    for t in gTestfiles:
        add_test_items(t, spec=None)

    listfile = open(os.path.join(gDestPath, "reftest.list"), "wb")
    listfile.write("# THIS FILE IS AUTOGENERATED BY {0}\n# DO NOT EDIT!\n".format(os.path.basename(__file__)))
    lastDefaultPreferences = None
    for test in tests:
        defaultPreferences = gDefaultPreferences.get(test[1].split("/")[0], None)
        if defaultPreferences != lastDefaultPreferences:
            if defaultPreferences is None:
                listfile.write("\ndefault-preferences\n\n")
            else:
                listfile.write("\ndefault-preferences {0}\n\n".format(defaultPreferences))
            lastDefaultPreferences = defaultPreferences
        key = 1
        while not test[key] in gTestFlags.keys() and key < len(test):
            key = key + 1
        testType = test[key - 1]
        testFlags = gTestFlags[test[key]]
        # Replace the Windows separators if any. Our internal strings
        # all use the system separator, however the failure/skip lists
        # and reftest.list always use '/' so we fix the paths here.
        test[key] = to_unix_path_sep(test[key])
        test[key + 1] = to_unix_path_sep(test[key + 1])
        testKey = test[key]
        if 'ahem' in testFlags:
            test = ["HTTP(../../..)"] + test
        fail = []
        for pattern, failureType in gFailList:
            if pattern.match(testKey):
                fail = failureType
        test = fail + test
        listfile.write(" ".join(test) + "\n")
    listfile.close()

    gLog.close()

if __name__ == '__main__':
    main()
