#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import subprocess

repo = "https://github.com/w3c/testharness.js"
dest = "resources-upstream"
files = [{"f":"testharness.js"},
         {"f":"testharness.css"},
         {"f":"idlharness.js"},
         {"d":"webidl2/lib/webidl2.js", "f":"WebIDLParser.js"}]

subprocess.check_call(["git", "clone", repo, dest])
subprocess.check_call(["git", "submodule", "init"], cwd=dest)
subprocess.check_call(["git", "submodule", "update"], cwd=dest)
for f in files:
    path = f["d"] if "d" in f else f["f"]
    subprocess.check_call(["cp", "%s/%s" % (dest, path), f["f"]])
    subprocess.check_call(["hg", "add", f["f"]])
subprocess.check_call(["rm", "-rf", dest])

