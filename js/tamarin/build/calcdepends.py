#!/usr/bin/env python

import os
from stat import *

class StatCache:
    def __init__(self):
	self._dict = {}

    def getStat(self, key):
	if not key in self._dict:
	    try:
		self._dict[key] = os.stat(key)
	    except OSError:
		self._dict[key] = None

	return self._dict[key]

    def getMTime(self, key):
	s = self.getStat(key);
	if s == None:
	    return 0;

	return s.st_mtime

_statcache = StatCache()

def rebuildNeeded(file, dependencies):
    """Calculate whether a file needs to be rebuilt by comparing its timestamp
    against a list of dependencies.

    returns True or False"""

    f = _statcache.getMTime(file)
    if f == 0:
	return True

    for dep in dependencies:
	d = _statcache.getMTime(dep)
	if d == 0 or d > f:
	    return True

    return False

def rebuildsNeeded(files, outfile):
    """Write a makefile snippet indicating whether object files need to be
    rebuilt.

    @param files: a dictionary of { 'objfile', 'depfile' }"""

    ostream = open(outfile, "w")

    for (objfile, depfile) in files.items():
	rebuild = True

	try:
	    d = open(depfile, "r")
	    rebuild = \
		rebuildNeeded(objfile, \
			      [line.rstrip("\n\r") for line in d.readlines()])
	    d.close()

	except IOError:
	    pass

	if rebuild:
	    ostream.write(objfile + ": FORCE\n")

    ostream.close()

if __name__ == "__main__":
    import sys
    import re

    _argExpr = re.compile("\\.(ii)$")

    files = {}

    sys.argv.pop(0)
    outfile = sys.argv.pop(0)

    for objfile in sys.argv:
	depfile = _argExpr.sub(".deps", objfile)
	files[objfile] = depfile

    rebuildsNeeded(files, outfile)
