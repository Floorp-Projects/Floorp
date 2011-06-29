"""
Run a python script, adding extra directories to the python path.
"""

import sys, os

def usage():
    print >>sys.stderr, "pythonpath.py -I directory script.py [args...]"
    sys.exit(150)

paths = []

while True:
    try:
        arg = sys.argv[1]
    except IndexError:
        usage()

    if arg == '-I':
        del sys.argv[1]
        try:
            path = sys.argv.pop(1)
        except IndexError:
            usage()

        paths.append(path)
        continue

    if arg.startswith('-I'):
        path = sys.argv.pop(1)[2:]
        paths.append(path)
        continue

    break

sys.argv.pop(0)
script = sys.argv[0]

sys.path[0:0] = [os.path.dirname(script)] + paths
__name__ = '__main__'
__file__ = script
execfile(script)
