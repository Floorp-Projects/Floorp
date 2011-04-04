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
# The Original Code is Property file to C++ array conversion code.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Henri Sivonen <hsivonen@iki.fi>
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

