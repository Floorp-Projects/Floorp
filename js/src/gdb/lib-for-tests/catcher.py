# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Apparently, there's simply no way to ask GDB to exit with a non-zero
# status when the script run with the --eval-command option fails. Thus, if
# we have --eval-command run prologue.py directly, syntax errors there will
# lead GDB to exit with no indication anything went wrong.
#
# To avert that, we use this very small launcher script to run prologue.py
# and catch errors.
#
# Remember, errors in this file will cause spurious passes, so keep this as
# simple as possible!
# flake8: noqa: F821

import os
import sys
import traceback


def execfile(filename, globs, locs):
    with open(filename) as f:
        code = compile(f.read(), filename, "exec")
        exec(code, globs, locs)


try:
    # testlibdir is set on the GDB command line, via:
    # --eval-command python testlibdir=...
    execfile(os.path.join(testlibdir, "prologue.py"), globals(), locals())
except Exception as err:
    sys.stderr.write("Error running GDB prologue:\n")
    traceback.print_exc()
    sys.exit(1)
