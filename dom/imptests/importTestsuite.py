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

def readManifests(iden, dirs):
    def parseManifestFile(iden, path):
        pathstr = "hg-%s/%s/MANIFEST" % (iden, path)
        subdirs, mochitests, _, __, supportfiles = parseManifest.parseManifestFile(pathstr)
        return subdirs, mochitests, supportfiles

    data = []
    for path in dirs:
        subdirs, mochitests, supportfiles = parseManifestFile(iden, path)
        data.append({
          "path": path,
          "mochitests": mochitests,
          "supportfiles": supportfiles,
        })
        data.extend(readManifests(iden, ["%s/%s" % (path, d) for d in subdirs]))
    return data


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


def makePathInternal(a, b):
    if not b:
        # Empty directory, i.e., the repository root.
        return a
    return "%s/%s" % (a, b)


def makeSourcePath(a, b):
    """Make a path in the source (upstream) directory."""
    return makePathInternal("hg-%s" % a, b)


def makeDestPath(a, b):
    """Make a path in the destination (mozilla-central) directory, shortening as
    appropriate."""
    def shorten(path):
        path = path.replace('dom-tree-accessors', 'dta')
        path = path.replace('document.getElementsByName', 'doc.gEBN')
        path = path.replace('requirements-for-implementations', 'implreq')
        path = path.replace('other-elements-attributes-and-apis', 'oeaaa')
        return path

    return shorten(makePathInternal(a, b))


def copy(dest, directories):
    """Copy mochitests and support files from the external HG directory to their
    place in mozilla-central.
    """
    print("Copying tests...")
    for d in directories:
        sourcedir = makeSourcePath(dest, d["path"])
        destdir = makeDestPath(dest, d["path"])
        os.makedirs(destdir)

        for mochitest in d["mochitests"]:
            shutil.copy("%s/%s" % (sourcedir, mochitest), "%s/test_%s" % (destdir, mochitest))
        for support in d["supportfiles"]:
            shutil.copy("%s/%s" % (sourcedir, support), "%s/%s" % (destdir, support))

def printMozbuildFile(dest, directories):
    """Create a .mozbuild file to be included into the main moz.build, which
    lists the directories with tests.
    """
    print("Creating mozbuild...")
    path = dest + ".mozbuild"
    with open(path, 'w') as fh:
        normalized = [makeDestPath(dest, d["path"]) for d in directories]
        result = writeBuildFiles.substMozbuild("importTestsuite.py",
            normalized)
        fh.write(result)

    subprocess.check_call(["hg", "add", path])

def printBuildFiles(dest, directories):
    """Create Makefile.in files for each directory that contains tests we import.
    """
    print("Creating build files...")
    for d in directories:
        path = makeDestPath(dest, d["path"])

        files = ["test_%s" % (mochitest, ) for mochitest in d["mochitests"]]
        files.extend(d["supportfiles"])

        with open(path + "/Makefile.in", "w") as fh:
            result = writeBuildFiles.substMakefile("importTestsuite.py", files)
            fh.write(result)

        with open(path + "/moz.build", "w") as fh:
            result = writeBuildFiles.substMozbuild("importTestsuite.py", [])
            fh.write(result)


def hgadd(dest, directories):
    """Inform hg of the files in |directories|."""
    print("hg addremoving...")
    for d in directories:
        subprocess.check_call(["hg", "addremove", makeDestPath(dest, d)])

def removeAndCloneRepo(vcs, url, dest):
    """Replaces the repo at dest by a fresh clone from url using vcs"""
    assert vcs in ('hg', 'git')

    print("Removing %s..." % dest)
    subprocess.check_call(["rm", "-rf", dest])

    print("Cloning %s to %s with %s..." % (url, dest, vcs))
    subprocess.check_call([vcs, "clone", url, dest])

def importRepo(confFile):
    try:
        vcs, url, iden, directories = getData(confFile)
        dest = iden
        hgdest = "hg-%s" % iden

        print("Removing %s..." % dest)
        subprocess.check_call(["rm", "-rf", dest])

        removeAndCloneRepo(vcs, url, hgdest)

        data = readManifests(iden, directories)
        print("Going to import %s..." % [d["path"] for d in data])

        copy(dest, data)
        printBuildFiles(dest, data)
        printMozbuildFile(dest, data)
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
        importRepo(sys.argv[1])

