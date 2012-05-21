# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Filename globbing like the python glob module with minor differences:

* glob relative to an arbitrary directory
* include . and ..
* check that link targets exist, not just links
"""

import os, re, fnmatch
import util

_globcheck = re.compile('[[*?]')

def hasglob(p):
    return _globcheck.search(p) is not None

def glob(fsdir, path):
    """
    Yield paths matching the path glob. Sorts as a bonus. Excludes '.' and '..'
    """

    dir, leaf = os.path.split(path)
    if dir == '':
        return globpattern(fsdir, leaf)

    if hasglob(dir):
        dirsfound = glob(fsdir, dir)
    else:
        dirsfound = [dir]

    r = []

    for dir in dirsfound:
        fspath = util.normaljoin(fsdir, dir)
        if not os.path.isdir(fspath):
            continue

        r.extend((util.normaljoin(dir, found) for found in globpattern(fspath, leaf)))

    return r

def globpattern(dir, pattern):
    """
    Return leaf names in the specified directory which match the pattern.
    """

    if not hasglob(pattern):
        if pattern == '':
            if os.path.isdir(dir):
                return ['']
            return []

        if os.path.exists(util.normaljoin(dir, pattern)):
            return [pattern]
        return []

    leaves = os.listdir(dir) + ['.', '..']

    # "hidden" filenames are a bit special
    if not pattern.startswith('.'):
        leaves = [leaf for leaf in leaves
                  if not leaf.startswith('.')]

    leaves = fnmatch.filter(leaves, pattern)
    leaves = filter(lambda l: os.path.exists(util.normaljoin(dir, l)), leaves)

    leaves.sort()
    return leaves
