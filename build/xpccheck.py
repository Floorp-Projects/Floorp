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
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Joel Maher <joel.maher@gmail.com>
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

'''A generic script to verify all test files are in the 
corresponding .ini file.

Usage: xpccheck.py <directory> [<directory> ...]
'''

import sys
import os
from glob import glob
import manifestparser

class srcManifestParser(manifestparser.ManifestParser):
  def __init__(self, manifests=(), defaults=None, strict=True, testroot=None):
    self.testroot = testroot
    manifestparser.ManifestParser.__init__(self, manifests, defaults, strict)

  def getRelativeRoot(self, here):
    if self.testroot is None:
        return manifestparser.ManifestParser.getRelativeRoot(self, self.rootdir)
    return self.testroot


def getIniTests(testdir):
  mp = manifestparser.ManifestParser(strict=False)
  mp.read(os.path.join(testdir, 'xpcshell.ini'))
  return mp.tests

def verifyDirectory(initests, directory):
  files = glob(os.path.join(os.path.abspath(directory), "test_*"))
  for f in files:
    if (not os.path.isfile(f)):
      continue

    name = os.path.basename(f)
    if name.endswith('.in'):
      name = name[:-3]

    if not name.endswith('.js'):
      continue

    found = False
    for test in initests:
      if os.path.join(os.path.abspath(directory), name) == test['path']:
        found = True
        break
   
    if not found:
      print >>sys.stderr, "TEST-UNEXPECTED-FAIL | xpccheck | test %s is missing from test manifest %s!" % (name, os.path.join(directory, 'xpcshell.ini'))
      sys.exit(1)

def verifyIniFile(initests, directory):
  files = glob(os.path.join(os.path.abspath(directory), "test_*"))
  for test in initests:
    name = test['path'].split('/')[-1]

    found = False
    for f in files:

      fname = f.split('/')[-1]
      if fname.endswith('.in'):
        fname = '.in'.join(fname.split('.in')[:-1])

      if os.path.join(os.path.abspath(directory), fname) == test['path']:
        found = True
        break

    if not found:
      print >>sys.stderr, "TEST-UNEXPECTED-FAIL | xpccheck | found %s in xpcshell.ini and not in directory '%s'" % (name, directory)
      sys.exit(1)

def verifyMasterIni(mastername, topsrcdir, directory):
  mp = srcManifestParser(strict=False, testroot=topsrcdir)
  mp.read(mastername)
  tests = mp.tests

  found = False
  for test in tests:
    if test['manifest'] == os.path.abspath(os.path.join(directory, 'xpcshell.ini')):
      found = True
      break

  if not found:
    print >>sys.stderr, "TEST-UNEXPECTED-FAIL | xpccheck | directory %s is missing from master xpcshell.ini file %s" % (directory, mastername)
    sys.exit(1)


if __name__ == '__main__':
  if len(sys.argv) < 4:
    print >>sys.stderr, "Usage: xpccheck.py <topsrcdir> <path/master.ini> <directory> [<directory> ...]"
    sys.exit(1)

  topsrcdir = sys.argv[1]
  for d in sys.argv[3:]:
    # xpcshell-unpack is a copy of xpcshell sibling directory and in the Makefile
    # we copy all files (including xpcshell.ini from the sibling directory.
    if d.endswith('toolkit/mozapps/extensions/test/xpcshell-unpack'):
      continue

    initests = getIniTests(d)
    verifyDirectory(initests, d)
    verifyIniFile(initests, d)
    verifyMasterIni(sys.argv[2], topsrcdir, d)


