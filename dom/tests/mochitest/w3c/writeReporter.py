# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import string

try:
  import json
except ImportError:
  import simplejson as json

def writeReporter(src):
  src = src.replace("testharnessreport.js.in", "")
  expectations = {}
  fp = open(src + "failures.txt", "rb")
  for line in fp:
    gp = open(src + line.strip() + ".json")
    expectations.update(json.load(gp))
    gp.close()
  fp.close()
  fp = open(src + "testharnessreport.js.in", "rb")
  template = fp.read()
  fp.close()
  fp = open("testharnessreport.js", "wb")
  expjson = json.dumps(expectations,
                       indent = 2,
                       sort_keys = True,
                       separators = (',', ': '))
  result = string.Template(template).substitute({ 'expectations': expjson })
  fp.write(result)
  fp.close()

if __name__ == "__main__":
  writeReporter(sys.argv[1])
