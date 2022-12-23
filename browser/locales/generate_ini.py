# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Generate updater.ini by doing some light substitution on the localized updater.ini input,
# and appending the contents of updater_ini_append on Windows.

import codecs
import re
import shutil

import buildconfig


def main(output, ini, ini_append=None, locale=None):
    fixup_re = re.compile("^(Info|Title)Text=")
    # Input INI is always utf-8.
    with codecs.open(ini, "rb", "utf_8") as f:
        for line in f:
            line = fixup_re.sub(r"\1=", line)
            line = line.replace(
                "%MOZ_APP_DISPLAYNAME%", buildconfig.substs["MOZ_APP_DISPLAYNAME"]
            )
            output.write(line)
    if ini_append and buildconfig.substs["OS_TARGET"] == "WINNT":
        # Also append the contents of `ini_append`.
        with codecs.open(ini_append, "rb", "utf_8") as f:
            shutil.copyfileobj(f, output)
