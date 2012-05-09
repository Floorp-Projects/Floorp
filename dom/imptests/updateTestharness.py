#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess

repo = "https://dvcs.w3.org/hg/resources"
dest = "resources-upstream"
files = ["testharness.js", "testharness.css", "idlharness.js", "WebIDLParser.js"]

subprocess.check_call(["hg", "clone", repo, dest])
for f in files:
  subprocess.check_call(["cp", "%s/%s" % (dest, f), f])
  subprocess.check_call(["hg", "add", f])
subprocess.check_call(["rm", "--recursive", "--force", dest])
