# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import platform
import sys

# TODO Bug 794506 Integrate with the in-tree virtualenv configuration.
SEARCH_PATHS = [
    'python/mach',
    'python/mozboot',
    'python/mozbuild',
    'python/blessings',
    'python/psutil',
    'python/which',
    'build/pymake',
    'config',
    'other-licenses/ply',
    'xpcom/idl-parser',
    'testing',
    'testing/xpcshell',
    'testing/mozbase/mozcrash',
    'testing/mozbase/mozlog',
    'testing/mozbase/mozprocess',
    'testing/mozbase/mozfile',
    'testing/mozbase/mozinfo',
]

# Individual files providing mach commands.
MACH_MODULES = [
    'addon-sdk/mach_commands.py',
    'layout/tools/reftest/mach_commands.py',
    'python/mach/mach/commands/commandinfo.py',
    'python/mozboot/mozboot/mach_commands.py',
    'python/mozbuild/mozbuild/config.py',
    'python/mozbuild/mozbuild/mach_commands.py',
    'python/mozbuild/mozbuild/frontend/mach_commands.py',
    'testing/mochitest/mach_commands.py',
    'testing/xpcshell/mach_commands.py',
    'tools/mach_commands.py',
]

def bootstrap(topsrcdir, mozilla_dir=None):
    if mozilla_dir is None:
        mozilla_dir = topsrcdir

    # Ensure we are running Python 2.7+. We put this check here so we generate a
    # user-friendly error message rather than a cryptic stack trace on module
    # import.
    if sys.version_info[0] != 2 or sys.version_info[1] < 7:
        print('Python 2.7 or above (but not Python 3) is required to run mach.')
        print('You are running Python', platform.python_version())
        sys.exit(1)

    try:
        import mach.main
    except ImportError:
        sys.path[0:0] = [os.path.join(mozilla_dir, path) for path in SEARCH_PATHS]
        import mach.main

    mach = mach.main.Mach(topsrcdir)
    for path in MACH_MODULES:
        mach.load_commands_from_file(os.path.join(mozilla_dir, path))
    return mach
