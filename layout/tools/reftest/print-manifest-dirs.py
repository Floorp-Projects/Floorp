#
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ted Mielczarek <ted.mielczarek@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import sys, os.path, re

commentRE = re.compile(r"\s+#")
conditionsRE = re.compile(r"^(fails|needs-focus|random|skip|asserts|slow|require-or|silentfail|pref|test-pref|ref-pref|fuzzy)")
httpRE = re.compile(r"HTTP\((\.\.(\/\.\.)*)\)")
protocolRE = re.compile(r"^\w+:")

def parseManifest(manifest, dirs):
  """Parse the reftest manifest |manifest|, adding all directories containing
  tests (and the dirs containing the manifests themselves) to the set |dirs|."""
  manifestdir = os.path.dirname(os.path.abspath(manifest))
  dirs.add(manifestdir)
  f = file(manifest)
  urlprefix = ''
  for line in f:
    if line[0] == '#':
      continue # entire line was a comment
    m = commentRE.search(line)
    if m:
      line = line[:m.start()]
    line = line.strip()
    if not line:
      continue
    items = line.split()
    while conditionsRE.match(items[0]):
      del items[0]
    if items[0] == "HTTP":
      del items[0]
    m = httpRE.match(items[0])
    if m:
      # need to package the dir referenced here
      d = os.path.normpath(os.path.join(manifestdir, m.group(1)))
      dirs.add(d)
      del items[0]

    if items[0] == "url-prefix":
      urlprefix = items[1]
      continue
    elif items[0] == "include":
      parseManifest(os.path.join(manifestdir, items[1]), dirs)
      continue
    elif items[0] == "load" or items[0] == "script":
      testURLs = [items[1]]
    elif items[0] == "==" or items[0] == "!=":
      testURLs = items[1:3]
    for u in testURLs:
      m = protocolRE.match(u)
      if m:
        # can't very well package about: or data: URIs
        continue
      d = os.path.dirname(os.path.normpath(os.path.join(manifestdir, urlprefix + u)))
      dirs.add(d)
  f.close()

def printTestDirs(topsrcdir, topmanifests):
  """Parse |topmanifests| and print a list of directories containing the tests
  within (and the manifests including those tests), relative to |topsrcdir|."""
  topsrcdir = os.path.abspath(topsrcdir)
  dirs = set()
  for manifest in topmanifests:
    parseManifest(manifest, dirs)
  for dir in sorted(dirs):
    d = dir[len(topsrcdir):].replace('\\','/')
    if d[0] == '/':
      d = d[1:]
    print d

if __name__ == '__main__':
  if len(sys.argv) < 3:
    print >>sys.stderr, "Usage: %s topsrcdir reftest.list [reftest.list]*" % sys.argv[0]
    sys.exit(1)
  printTestDirs(sys.argv[1], sys.argv[2:])
