# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import configobj
import sys
import re
from StringIO import StringIO

try:
    (file, section, key) = sys.argv[1:]
except ValueError:
    print("Usage: printconfigsetting.py <file> <section> <setting>")
    sys.exit(1)

with open(file) as fh:
    content = re.sub('^\s*;', '#', fh.read(), flags=re.M)

c = configobj.ConfigObj(StringIO(content))

try:
    s = c[section]
except KeyError:
    print >>sys.stderr, "Section [%s] not found." % section
    sys.exit(1)

try:
    print(s[key])
except KeyError:
    print >>sys.stderr, "Key %s not found." % key
    sys.exit(1)
