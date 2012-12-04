# Apparently, there's simply no way to ask GDB to exit with a non-zero
# status when the script run with the --python option fails. Thus, if we
# have --python run prolog.py directly, syntax errors there will lead GDB
# to exit with no indication anything went wrong.
#
# To avert that, we use this very small launcher script to run prolog.py
# and catch errors.
#
# Remember, errors in this file will cause spurious passes, so keep this as
# simple as possible!

import sys
import traceback
try:
    execfile(sys.argv.pop(0))
except Exception as err:
    sys.stderr.write('Error running GDB prologue:\n')
    traceback.print_exc()
    sys.exit(1)
