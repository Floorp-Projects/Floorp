# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

mappings = {}
  
propFile = open(sys.argv[1], "r");
  
for line in propFile:
  line = line.strip()
  if not line.startswith('#'):
    parts = line.split("=", 1)
    if len(parts) == 2 and len(parts[0]) > 0:
      mappings[parts[0].strip()] = parts[1].strip()
 
propFile.close()

keys = mappings.keys()
keys.sort()

hFile = open(sys.argv[2], "w");

hFile.write("// This is a generated file. Please do not edit.\n")
hFile.write("// Please edit the corresponding .properties file instead.\n")

first = 1
for key in keys:
  if first:
    first = 0
  else:
    hFile.write(',\n')
  hFile.write('{ "%s", "%s", (const char*)NS_INT32_TO_PTR(%d) }' 
              % (key, mappings[key], len(mappings[key])));
hFile.write('\n')
hFile.flush()
hFile.close()

