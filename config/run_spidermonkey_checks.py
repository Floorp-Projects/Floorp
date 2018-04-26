# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig
import subprocess
import sys

def main(output, lib_file, *scripts):
    for script in scripts:
        retcode = subprocess.call([sys.executable, script], cwd=buildconfig.topsrcdir)
        if retcode != 0:
            raise Exception(script + " failed")
