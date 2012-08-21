#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# FIXME:
#   * Import more tests rather than just the very limited set currently
#     chosen.
#   * Read in a (checked-in) input file with a list of test assertions
#     expected to fail.
#   * Read in a (checked-in) input file with a list of reference choices
#     for tests with multiple rel="match" references.  (But still go
#     though all those references in case they, in turn, have references.)

import os
from optparse import OptionParser
from subprocess import Popen, PIPE
import xml.dom.minidom
import html5lib
import shutil

op = OptionParser()
(options, args) = op.parse_args()

if len(args) != 1:
    op.error("expected a single argument (path to CSS test repository clone)")

srcpath = args[0]
if not os.path.isdir(srcpath) or \
   not os.path.isdir(os.path.join(srcpath, ".hg")):
    raise StandardError("source path does not appear to be a mercurial clone")

destpath = os.path.join(os.path.dirname(os.path.realpath(__file__)), "received")

# Since we're going to commit the tests, we should also commit
# information about where they came from.
log = open(os.path.join(destpath, "import.log"), "w")

def log_output_of(subprocess):
    subprocess.wait()
    if (subprocess.returncode != 0):
        raise StandardError("error while running subprocess")
    log.write(subprocess.stdout.readline().rstrip())
log.write("Importing revision: ")
log_output_of(Popen(["hg", "parent", "--template={node}"],
                    stdout=PIPE, cwd=srcpath))
log.write("\nfrom repository: ")
log_output_of(Popen(["hg", "paths", "default"],
                    stdout=PIPE, cwd=srcpath))
log.write("\n")

# Remove existing directories that we're going to regenerate.  This
# is necessary so that we can give errors in cases where our import
# might copy two files to the same location, which we do by giving
# errors if a copy would overwrite a file.
for dirname in os.listdir(destpath):
    fulldir = os.path.join(destpath, dirname)
    if not os.path.isdir(fulldir):
        continue
    shutil.rmtree(fulldir)

# Eventually we should import all the tests that have references.  (At
# least for a subset of secs.  And we probably want to organize the
# directory structure by spec to avoid constant file moves when files
# move in the W3C repository.  And we probably also want to import each
# test only once, even if it covers more than one spec.)

# But for now, let's just import one set of tests.

subtrees = [
    os.path.join(srcpath, "approved", "css3-namespace", "src"),
    os.path.join(srcpath, "contributors", "opera", "submitted", "css3-conditional")
]
testfiles = []
for subtree in subtrees:
    for dirpath, dirnames, filenames in os.walk(subtree, topdown=True):
        if "support" in dirnames:
           dirnames.remove("support")
        if "reftest" in dirnames:
           dirnames.remove("reftest")
        for f in filenames:
            if f == "README" or \
               f.find("-ref.") != -1:
                continue
            testfiles.append(os.path.join(dirpath, f))

testfiles.sort()

def copy_file(srcfile, destname):
    if not srcfile.startswith(srcpath):
        raise StandardError("Filename " + srcfile + " does not start with " + srcpath)
    logname = srcfile[len(srcpath):]
    log.write("Importing " + logname + " to " + destname + "\n")
    destfile = os.path.join(destpath, destname)
    destdir = os.path.dirname(destfile)
    if not os.path.exists(destdir):
        os.makedirs(destdir)
    if os.path.exists(destfile):
        raise StandardError("file " + destfile + " already exists")
    shutil.copyfile(srcfile, destfile)

support_dirs_mapped = set()
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

filemap = {}
def map_file(fn, spec):
    if fn in filemap:
        return filemap[fn]
    destname = os.path.join(spec, os.path.basename(fn))
    filemap[fn] = destname
    copy_file(fn, destname)
    copy_support_files(os.path.dirname(fn), spec)
    return destname

tests = []
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

for t in testfiles:
    add_test_items(t, spec=None)

listfile = open(os.path.join(destpath, "reftest.list"), "w")
for test in tests:
    listfile.write(" ".join(test) + "\n")
listfile.close()

log.close()
