# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generate updater.ini by doing some light substitution on the localized updater.ini input,
# and appending the contents of updater_ini_append on Windows.

from __future__ import absolute_import, unicode_literals, print_function

import buildconfig
import codecs
import re
import shutil

def main(output, updater_ini, updater_ini_append, locale=None):
    assert(locale is not None)
    fixup_re = re.compile('^(Info|Title)Text=')
    # updater.ini is always utf-8.
    with codecs.open(updater_ini, 'rb', 'utf_8') as f:
        for line in f:
            line = fixup_re.sub(r'\1=', line)
            line = line.replace('%MOZ_APP_DISPLAYNAME%', buildconfig.substs['MOZ_APP_DISPLAYNAME'])
            output.write(line)
    if buildconfig.substs['OS_TARGET'] == 'WINNT':
        # Also append the contents of `updater_ini_append`.
        with codecs.open(updater_ini_append, 'rb', 'utf_8') as f:
            shutil.copyfileobj(f, output)
