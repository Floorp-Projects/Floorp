#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Write a Mochitest manifest for WebGL conformance test files.

import os
from itertools import chain

CONFORMANCE_DIRS = [
    "conformance",
    "resources",
]

def listfiles(dir, rel):
    """List all files in dir recursively, yielding paths
    relative to rel.
    """
    for root, folders, files in os.walk(dir):
        for f in files:
            yield os.path.relpath(os.path.join(root, f), rel)

def writemanifest():
    script_dir = os.path.dirname(__file__)
    list_dirs = [os.path.join(script_dir, d) for d in CONFORMANCE_DIRS]
    with open(os.path.join(script_dir, 'mochitest-conformance-files.ini'), 'w') as f:
        f.write("""[DEFAULT]
support-files =
  %s
""" % "\n  ".join(sorted(chain.from_iterable(listfiles(d, script_dir)
                                             for d in list_dirs))))

if __name__ == '__main__':
    writemanifest()

