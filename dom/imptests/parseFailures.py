# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys

import writeMakefile

def extractLines(fp):
    lines = []
    watch = False
    for line in fp:
        if line == '@@@ @@@ Failures\n':
            watch = True
        elif watch:
            watch = False
            idx = line.index('@@@')
            lines.append((line[:idx], line[idx + 3:]))
    return lines

def ensuredir(path):
    dir = path[:path.rfind('/')]
    if not os.path.exists(dir):
        os.makedirs(dir)

def dumpFailures(lines):
    files = []
    for url, objstr in lines:
        if objstr == '{}\n':
            continue

        jsonpath = 'failures/' + url + '.json'
        files.append(jsonpath)
        ensuredir(jsonpath)
        obj = json.loads(objstr)
        formattedobj = json.dumps(obj, sort_keys=True, indent=2, separators=(',', ': '))
        fp = open(jsonpath, 'w')
        fp.write(formattedobj + '\n')
        fp.close()
    return files

def writeMakefiles(files):
    pathmap = {}
    for path in files:
        dirp, leaf = path.rsplit('/', 1)
        pathmap.setdefault(dirp, []).append(leaf)

    for k, v in pathmap.iteritems():
        result = writeMakefile.substMakefile('parseFailures.py', 'dom/imptests/' + k, [], v)

        fp = open(k + '/Makefile.in', 'wb')
        fp.write(result)
        fp.close()

def main(logPath):
    fp = open(logPath, 'rb')
    lines = extractLines(fp)
    fp.close()

    files = dumpFailures(lines)
    writeMakefiles(files)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "Please pass the path to the logfile from which failures should be extracted."
    main(sys.argv[1])
