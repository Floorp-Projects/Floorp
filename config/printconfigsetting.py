# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function

import configobj, sys

try:
    (file, section, key) = sys.argv[1:]
except ValueError:
    print("Usage: printconfigsetting.py <file> <section> <setting>")
    sys.exit(1)

c = configobj.ConfigObj(file)

try:
    s = c[section]
except KeyError:
    print("Section [{0}] not found.".format(section), file=sys.stderr)
    sys.exit(1)

try:
    print(s[key])
except KeyError:
    print("Key {0} not found.".format(key), file=sys.stderr)
    sys.exit(1)
