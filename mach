#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The beginning of this script is both valid shell and valid python,
# such that the script starts with the shell and is reexecuted with
# the right python.
'''which' python2.7 > /dev/null && exec python2.7 "$0" "$@" || exec python "$0" "$@"
'''

from __future__ import print_function, unicode_literals

import os
import sys

def ancestors(path):
    while path:
        yield path
        (path, child) = os.path.split(path)
        if child == "":
            break

def load_mach(dir_path, mach_path):
    import imp
    with open(mach_path, 'r') as fh:
        imp.load_module('mach_bootstrap', fh, mach_path,
                        ('.py', 'r', imp.PY_SOURCE))
    import mach_bootstrap
    return mach_bootstrap.bootstrap(dir_path)


def check_and_get_mach(dir_path):
    bootstrap_paths = (
        'build/mach_bootstrap.py',
        # test package bootstrap
        'tools/mach_bootstrap.py',
    )
    for bootstrap_path in bootstrap_paths:
        mach_path = os.path.join(dir_path, bootstrap_path)
        if os.path.isfile(mach_path):
            return load_mach(dir_path, mach_path)
    return None


def get_mach():
    # Check whether the current directory is within a mach src or obj dir.
    for dir_path in ancestors(os.getcwd()):
        # If we find a "config.status" and "mozinfo.json" file, we are in the objdir.
        config_status_path = os.path.join(dir_path, 'config.status')
        mozinfo_path = os.path.join(dir_path, 'mozinfo.json')
        if os.path.isfile(config_status_path) and os.path.isfile(mozinfo_path):
            import json
            info = json.load(open(mozinfo_path))
            if 'mozconfig' in info and 'MOZCONFIG' not in os.environ:
                # If the MOZCONFIG environment variable is not already set, set it
                # to the value from mozinfo.json.  This will tell the build system
                # to look for a config file at the path in $MOZCONFIG rather than
                # its default locations.
                #
                # Note: subprocess requires native strings in os.environ on Windows
                os.environ[b'MOZCONFIG'] = str(info['mozconfig'])

            if 'topsrcdir' in info:
                # Continue searching for mach_bootstrap in the source directory.
                dir_path = info['topsrcdir']

        mach = check_and_get_mach(dir_path)
        if mach:
            return mach

    # If we didn't find a source path by scanning for a mozinfo.json, check
    # whether the directory containing this script is a source directory. We
    # follow symlinks so mach can be run even if cwd is outside the srcdir.
    return check_and_get_mach(os.path.dirname(os.path.realpath(__file__)))

def main(args):
    mach = get_mach()
    if not mach:
        print('Could not run mach: No mach source directory found.')
        sys.exit(1)
    sys.exit(mach.run(args))


if __name__ == '__main__':
    main(sys.argv[1:])
