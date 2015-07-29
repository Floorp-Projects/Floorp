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

import os
import sys
import traceback
try:
    # testlibdir is set on the GDB command line, via:
    # --eval-command python testlibdir=...
    exec(open(os.path.join(testlibdir, 'prologue.py')).read())
except Exception as err:
    sys.stderr.write('Error running GDB prologue:\n')
    traceback.print_exc()
    sys.exit(1)
