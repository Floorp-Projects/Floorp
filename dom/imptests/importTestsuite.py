#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Imports a test suite from a remote repository. Takes one argument, a file in
the format described in README.
Note: removes both source and destination directory before starting. Do not
      use with outstanding changes in either directory.
"""

from __future__ import print_function, unicode_literals

import os
import shutil
import subprocess
import sys

import parseManifest
import writeBuildFiles

HEADERS_SUFFIX = "^headers^"

def parseManifestFile(dest, dir):
    subdirs, mochitests, _, __, supportfiles = parseManifest.parseManifestFile("hg-%s/%s/MANIFEST" % (dest, dir))
    return subdirs, mochitests, supportfiles

def getData(confFile):
    """This function parses a file of the form
    (hg or git)|URL of remote repository|identifier for the local directory
    First directory of tests
    ...
    Last directory of tests"""
    vcs = ""
    url = ""
    iden = ""
    directories = []
    try:
        with open(confFile, 'r') as fp:
            first = True
            for line in fp:
                if first:
                    vcs, url, iden = line.strip().split("|")
                    first = False
                else:
                    directories.append(line.strip())
    finally:
        return vcs, url, iden, directories

def makePath(a, b):
    if not b:
        # Empty directory, i.e., the repository root.
        return a
    return "%s/%s" % (a, b)

def copyTest(source, dest):
    """Copy the file at source to dest, as well as any ^headers^ file associated
    with it."""
    shutil.copy(source, dest)
    if os.path.exists(source + HEADERS_SUFFIX):
        shutil.copy(source + HEADERS_SUFFIX, dest + HEADERS_SUFFIX)

def copy(thissrcdir, dest, directories):
    """Copy mochitests and support files from the external HG directory to their
    place in mozilla-central.
    """
    print("Copying %s..." % directories)
    for d in directories:
        subdirs, mochitests, supportfiles = parseManifestFile(dest, d)
        sourcedir = makePath("hg-%s" % dest, d)
        destdir = makePath(dest, d)
        os.makedirs(destdir)

        for mochitest in mochitests:
            copyTest("%s/%s" % (sourcedir, mochitest), "%s/test_%s" % (destdir, mochitest))
        for support in supportfiles:
            copyTest("%s/%s" % (sourcedir, support), "%s/%s" % (destdir, support))

        if len(subdirs):
            if d:
                importDirs(thissrcdir, dest, ["%s/%s" % (d, subdir) for subdir in subdirs])
            else:
                # Empty directory, i.e., the repository root
                importDirs(thissrcdir, dest, subdirs)

def printMozbuildFile(dest, directories):
    """Create a .mozbuild file to be included into the main moz.build, which
    lists the directories with tests.
    """
    print("Creating mozbuild...")
    path = dest + ".mozbuild"
    with open(path, 'w') as fh:
        normalized = [makePath(dest, d) for d in directories]
        result = writeBuildFiles.substMozbuild("importTestsuite.py",
            normalized)
        fh.write(result)

    subprocess.check_call(["hg", "add", path])

def printBuildFiles(thissrcdir, dest, directories):
    """Create Makefile.in files for each directory that contains tests we import.
    """
    print("Creating build files...")
    for d in directories:
        path = makePath(dest, d)
        print("Creating Makefile.in in %s..." % path)

        subdirs, mochitests, supportfiles = parseManifestFile(dest, d)

        files = ["test_%s" % (mochitest, ) for mochitest in mochitests]
        files.extend(supportfiles)
        files.extend(f for f in os.listdir(path) if f.endswith(HEADERS_SUFFIX))

        with open(path + "/Makefile.in", "w") as fh:
            result = writeBuildFiles.substMakefile("importTestsuite.py", files)
            fh.write(result)

        print("Creating moz.build in %s..." % path)
        with open(path + "/moz.build", "w") as fh:
            result = writeBuildFiles.substMozbuild("importTestsuite.py",
                subdirs)
            fh.write(result)


def hgadd(dest, directories):
    """Inform hg of the files in |directories|."""
    print("hg addremoving...")
    for d in directories:
        subprocess.check_call(["hg", "addremove", "%s/%s" % (dest, d)])

def importDirs(thissrcdir, dest, directories):
    copy(thissrcdir, dest, directories)
    printBuildFiles(thissrcdir, dest, directories)

def removeAndCloneRepo(vcs, url, dest):
    """Replaces the repo at dest by a fresh clone from url using vcs"""
    assert vcs in ('hg', 'git')

    print("Removing %s..." % dest)
    subprocess.check_call(["rm", "-rf", dest])

    print("Cloning %s to %s with %s..." % (url, dest, vcs))
    subprocess.check_call([vcs, "clone", url, dest])

def importRepo(confFile, thissrcdir):
    try:
        vcs, url, iden, directories = getData(confFile)
        dest = iden
        hgdest = "hg-%s" % iden

        print("Removing %s..." % dest)
        subprocess.check_call(["rm", "-rf", dest])

        removeAndCloneRepo(vcs, url, hgdest)

        print("Going to import %s..." % directories)
        importDirs(thissrcdir, dest, directories)
        printMozbuildFile(dest, directories)
        hgadd(dest, directories)
        print("Removing %s again..." % hgdest)
        subprocess.check_call(["rm", "-rf", hgdest])
    except subprocess.CalledProcessError as e:
        print(e.returncode)
    finally:
        print("Done")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Need one argument.")
    else:
        importRepo(sys.argv[1], "dom/imptests")

