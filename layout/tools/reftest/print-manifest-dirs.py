#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
