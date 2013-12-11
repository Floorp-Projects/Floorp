# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import platform
import sys
import time


STATE_DIR_FIRST_RUN = '''
mach and the build system store shared state in a common directory on the
filesystem. The following directory will be created:

  {userdir}

If you would like to use a different directory, hit CTRL+c and set the
MOZBUILD_STATE_PATH environment variable to the directory you would like to
use and re-run mach. For this change to take effect forever, you'll likely
want to export this environment variable from your shell's init scripts.
'''.lstrip()


# TODO Bug 794506 Integrate with the in-tree virtualenv configuration.
SEARCH_PATHS = [
    'python/mach',
    'python/mozboot',
    'python/mozbuild',
    'python/mozversioncontrol',
    'python/blessings',
    'python/configobj',
    'python/psutil',
    'python/which',
    'build/pymake',
    'config',
    'dom/bindings',
    'dom/bindings/parser',
    'other-licenses/ply',
    'xpcom/idl-parser',
    'testing',
    'testing/xpcshell',
    'testing/marionette/client',
    'testing/marionette/client/marionette',
    'testing/mozbase/mozcrash',
    'testing/mozbase/mozdevice',
    'testing/mozbase/mozfile',
    'testing/mozbase/mozhttpd',
    'testing/mozbase/mozlog',
    'testing/mozbase/moznetwork',
    'testing/mozbase/mozprocess',
    'testing/mozbase/mozprofile',
    'testing/mozbase/mozrunner',
    'testing/mozbase/mozsystemmonitor',
    'testing/mozbase/mozinfo',
    'testing/mozbase/moztest',
    'testing/mozbase/manifestdestiny',
    'xpcom/idl-parser',
]

# Individual files providing mach commands.
MACH_MODULES = [
    'addon-sdk/mach_commands.py',
    'dom/bindings/mach_commands.py',
    'layout/tools/reftest/mach_commands.py',
    'python/mach_commands.py',
    'python/mach/mach/commands/commandinfo.py',
    'python/mozboot/mozboot/mach_commands.py',
    'python/mozbuild/mozbuild/mach_commands.py',
    'python/mozbuild/mozbuild/frontend/mach_commands.py',
    'testing/mach_commands.py',
    'testing/marionette/mach_commands.py',
    'testing/mochitest/mach_commands.py',
    'testing/xpcshell/mach_commands.py',
    'testing/talos/mach_commands.py',
    'testing/xpcshell/mach_commands.py',
    'tools/mercurial/mach_commands.py',
    'tools/mach_commands.py',
]


CATEGORIES = {
    'build': {
        'short': 'Build Commands',
        'long': 'Interact with the build system',
        'priority': 80,
    },
    'post-build': {
        'short': 'Post-build Commands',
        'long': 'Common actions performed after completing a build.',
        'priority': 70,
    },
    'testing': {
        'short': 'Testing',
        'long': 'Run tests.',
        'priority': 60,
    },
    'devenv': {
        'short': 'Development Environment',
        'long': 'Set up and configure your development environment.',
        'priority': 50,
    },
    'build-dev': {
        'short': 'Low-level Build System Interaction',
        'long': 'Interact with specific parts of the build system.',
        'priority': 20,
    },
    'misc': {
        'short': 'Potpourri',
        'long': 'Potent potables and assorted snacks.',
        'priority': 10,
    },
    'disabled': {
        'short': 'Disabled',
        'long': 'These commands are unavailable for your current context, run "mach <command>" to see why.',
        'priority': 0,
    }
}


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

    # Global build system and mach state is stored in a central directory. By
    # default, this is ~/.mozbuild. However, it can be defined via an
    # environment variable. We detect first run (by lack of this directory
    # existing) and notify the user that it will be created. The logic for
    # creation is much simpler for the "advanced" environment variable use
    # case. For default behavior, we educate users and give them an opportunity
    # to react. We always exit after creating the directory because users don't
    # like surprises.
    state_user_dir = os.path.expanduser('~/.mozbuild')
    state_env_dir = os.environ.get('MOZBUILD_STATE_PATH', None)
    if state_env_dir:
        if not os.path.exists(state_env_dir):
            print('Creating global state directory from environment variable: %s'
                % state_env_dir)
            os.makedirs(state_env_dir, mode=0o770)
            print('Please re-run mach.')
            sys.exit(1)
        state_dir = state_env_dir
    else:
        if not os.path.exists(state_user_dir):
            print(STATE_DIR_FIRST_RUN.format(userdir=state_user_dir))
            try:
                for i in range(20, -1, -1):
                    time.sleep(1)
                    sys.stdout.write('%d ' % i)
                    sys.stdout.flush()
            except KeyboardInterrupt:
                sys.exit(1)

            print('\nCreating default state directory: %s' % state_user_dir)
            os.mkdir(state_user_dir)
            print('Please re-run mach.')
            sys.exit(1)
        state_dir = state_user_dir

    try:
        import mach.main
    except ImportError:
        sys.path[0:0] = [os.path.join(mozilla_dir, path) for path in SEARCH_PATHS]
        import mach.main

    def populate_context(context):
        context.state_dir = state_dir
        context.topdir = topsrcdir

    mach = mach.main.Mach(os.getcwd())
    mach.populate_context_handler = populate_context

    for category, meta in CATEGORIES.items():
        mach.define_category(category, meta['short'], meta['long'],
            meta['priority'])

    for path in MACH_MODULES:
        mach.load_commands_from_file(os.path.join(mozilla_dir, path))

    return mach
