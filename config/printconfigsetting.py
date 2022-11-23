# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from configparser import ConfigParser, NoOptionError, NoSectionError

try:
    (filename, section, key) = sys.argv[1:]
except ValueError:
    print("Usage: printconfigsetting.py <filename> <section> <setting>")
    sys.exit(1)

cfg = ConfigParser()
cfg.read(filename)

try:
    print(cfg.get(section, key))
except NoOptionError:
    print("Key %s not found." % key, file=sys.stderr)
    sys.exit(1)
except NoSectionError:
    print("Section [%s] not found." % section, file=sys.stderr)
    sys.exit(1)
