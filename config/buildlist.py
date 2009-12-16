# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla build system.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Ted Mielczarek <ted.mielczarek@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisiwons above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
