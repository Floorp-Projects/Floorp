#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from optparse import OptionParser
from subprocess import Popen, PIPE
import xml.dom.minidom
import html5lib
import shutil

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
    os.path.join("approved", "css3-namespace", "src"),
    #os.path.join("approved", "css3-multicol", "src"),
    os.path.join("contributors", "opera", "submitted", "css3-conditional"),
    #os.path.join("contributors", "opera", "submitted", "multicol")
]

gLog = None
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
    for subtree in gSubtrees:
        for dirpath, dirnames, filenames in os.walk(subtree, topdown=True):
            if "support" in dirnames:
               dirnames.remove("support")
            if "reftest" in dirnames:
               dirnames.remove("reftest")
            for f in filenames:
                if f == "README" or \
                   f.find("-ref.") != -1:
                    continue
                gTestfiles.append(os.path.join(dirpath, f))

    gTestfiles.sort()

def copy_file(srcfile, destname):
    global gDestPath, gLog, gSrcPath
    if not srcfile.startswith(gSrcPath):
        raise StandardError("Filename " + srcfile + " does not start with " + gSrcPath)
    logname = srcfile[len(gSrcPath):]
    gLog.write("Importing " + logname + " to " + destname + "\n")
    destfile = os.path.join(gDestPath, destname)
    destdir = os.path.dirname(destfile)
    if not os.path.exists(destdir):
        os.makedirs(destdir)
    if os.path.exists(destfile):
        raise StandardError("file " + destfile + " already exists")
    copy_and_prefix(srcfile, destfile, gPrefixedProperties)

def copy_support_files(dirname, spec):
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
            copy_file(full_fn, os.path.join(spec, "support", full_fn[len(support_dir)+1:]))

def map_file(fn, spec):
    if fn in filemap:
        return filemap[fn]
    destname = os.path.join(spec, os.path.basename(fn))
    filemap[fn] = destname
    copy_file(fn, destname)
    copy_support_files(os.path.dirname(fn), spec)
    return destname

def add_test_items(fn, spec):
    document = None # an xml.dom.minidom document
    if fn.endswith(".htm") or fn.endswith(".html"):
        # An HTML file
        f = open(fn, "r")
        parser = html5lib.HTMLParser(tree=html5lib.treebuilders.getTreeBuilder("dom"))
        document = parser.parse(f)
        f.close()
    else:
        # An XML file
        document = xml.dom.minidom.parse(fn)
    refs = []
    notrefs = []
    for link in document.getElementsByTagName("link"):
        rel = link.getAttribute("rel")
        if rel == "help" and spec == None:
            specurl = link.getAttribute("href")
            startidx = specurl.find("/TR/")
            if startidx != -1:
                startidx = startidx + 4
                endidx = specurl.find("/", startidx)
                if endidx != -1:
                    spec = str(specurl[startidx:endidx])
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
        raise StandardError("Could not associate test with specification")
    for ref in refs:
        tests.append(["==", map_file(fn, spec), map_file(ref, spec)])
    for notref in notrefs:
        tests.append(["!=", map_file(fn, spec), map_file(notref, spec)])
    # Add chained references too
    for ref in refs:
        add_test_items(ref, spec=spec)
    for notref in notrefs:
        add_test_items(notref, spec=spec)

def copy_and_prefix(aSourceFileName, aDestFileName, props):
    newFile = open(aDestFileName, 'w')
    unPrefixedFile = open(aSourceFileName)

    for line in unPrefixedFile:
        replacementLine = line;
        for rule in props:
            replacementLine = replacementLine.replace(rule, "-moz-" + rule)
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
    gLog = open(os.path.join(gDestPath, "import.log"), "w")

def main():
    global gDestPath, gLog, gTestfiles
    read_options()
    setup_paths()
    setup_log()
    write_log_header()
    remove_existing_dirs()
    populate_test_files()

    for t in gTestfiles:
        add_test_items(t, spec=None)

    listfile = open(os.path.join(gDestPath, "reftest.list"), "w")
    for test in tests:
        listfile.write(" ".join(test) + "\n")
    listfile.close()

    gLog.close()

if __name__ == '__main__':
    main()
