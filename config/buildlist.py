# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''A generic script to add entries to a file 
if the entry does not already exist.

Usage: buildlist.py <filename> <entry> [<entry> ...]
'''

import sys
import os
from utils import lockFile

def addEntriesToListFile(listFile, entries):
  """Given a file |listFile| containing one entry per line,
  add each entry in |entries| to the file, unless it is already
  present."""
  lock = lockFile(listFile + ".lck")
  try:
    if os.path.exists(listFile):
      f = open(listFile)
      existing = set([x.strip() for x in f.readlines()])
      f.close()
    else:
      existing = set()
    f = open(listFile, 'a')
    for e in entries:
      if e not in existing:
        f.write("%s\n" % e)
        existing.add(e)
    f.close()
  finally:
    lock = None

if __name__ == '__main__':
  if len(sys.argv) < 3:
    print >>sys.stderr, "Usage: buildlist.py <list file> <entry> [<entry> ...]"
    sys.exit(1)
  addEntriesToListFile(sys.argv[1], sys.argv[2:])
