# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import errno
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

NO_MERCURIAL_SETUP = '''
*** MERCURIAL NOT CONFIGURED ***

mach has detected that you have never run `mach mercurial-setup`.

Running this command will ensure your Mercurial version control tool is up
to date and optimally configured for a better, more productive experience
when working on Mozilla projects.

Please run `mach mercurial-setup` now.
'''.strip()

OLD_MERCURIAL_TOOLS = '''
*** MERCURIAL CONFIGURATION POTENTIALLY OUT OF DATE ***

mach has detected that it has been a while since you have run
`mach mercurial-setup`.

Having the latest Mercurial tools and configuration should lead to a better,
more productive experience when working on Mozilla projects.

Please run `mach mercurial-setup` now.

To avoid this message in the future, run `mach mercurial-setup` once a month.
Or, schedule `mach mercurial-setup --update-only` to run automatically in
the background at least once a month.
'''.strip()

MERCURIAL_SETUP_FATAL_INTERVAL = 31 * 24 * 60 * 60


# TODO Bug 794506 Integrate with the in-tree virtualenv configuration.
SEARCH_PATHS = [
    'python/mach',
    'python/mozboot',
    'python/mozbuild',
    'python/mozversioncontrol',
    'python/blessings',
    'python/compare-locales',
    'python/configobj',
    'python/jsmin',
    'python/psutil',
    'python/which',
    'python/pystache',
    'python/pyyaml/lib',
    'python/requests',
    'build',
    'build/pymake',
    'config',
    'dom/bindings',
    'dom/bindings/parser',
    'layout/tools/reftest',
    'other-licenses/ply',
    'xpcom/idl-parser',
    'testing',
    'testing/tools/autotry',
    'testing/taskcluster',
    'testing/xpcshell',
    'testing/web-platform',
    'testing/web-platform/harness',
    'testing/marionette/client',
    'testing/marionette/client/marionette/runner/mixins/browsermob-proxy-py',
    'testing/marionette/transport',
    'testing/marionette/driver',
    'testing/luciddream',
    'testing/mozbase/mozcrash',
    'testing/mozbase/mozdebug',
    'testing/mozbase/mozdevice',
    'testing/mozbase/mozfile',
    'testing/mozbase/mozhttpd',
    'testing/mozbase/mozleak',
    'testing/mozbase/mozlog',
    'testing/mozbase/moznetwork',
    'testing/mozbase/mozprocess',
    'testing/mozbase/mozprofile',
    'testing/mozbase/mozrunner',
    'testing/mozbase/mozsystemmonitor',
    'testing/mozbase/mozinfo',
    'testing/mozbase/moztest',
    'testing/mozbase/mozversion',
    'testing/mozbase/manifestparser',
    'xpcom/idl-parser',
]

# Individual files providing mach commands.
MACH_MODULES = [
    'addon-sdk/mach_commands.py',
    'build/valgrind/mach_commands.py',
    'dom/bindings/mach_commands.py',
    'layout/tools/reftest/mach_commands.py',
    'python/mach_commands.py',
    'python/mach/mach/commands/commandinfo.py',
    'python/compare-locales/mach_commands.py',
    'python/mozboot/mozboot/mach_commands.py',
    'python/mozbuild/mozbuild/mach_commands.py',
    'python/mozbuild/mozbuild/backend/mach_commands.py',
    'python/mozbuild/mozbuild/compilation/codecomplete.py',
    'python/mozbuild/mozbuild/frontend/mach_commands.py',
    'services/common/tests/mach_commands.py',
    'testing/luciddream/mach_commands.py',
    'testing/mach_commands.py',
    'testing/taskcluster/mach_commands.py',
    'testing/marionette/mach_commands.py',
    'testing/mochitest/mach_commands.py',
    'testing/xpcshell/mach_commands.py',
    'testing/talos/mach_commands.py',
    'testing/web-platform/mach_commands.py',
    'testing/xpcshell/mach_commands.py',
    'tools/docs/mach_commands.py',
    'tools/mercurial/mach_commands.py',
    'tools/mach_commands.py',
    'mobile/android/mach_commands.py',
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
    'ci': {
        'short': 'CI',
        'long': 'Taskcluster commands',
        'priority': 59
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
        'long': 'The disabled commands are hidden by default. Use -v to display them. These commands are unavailable for your current context, run "mach <command>" to see why.',
        'priority': 0,
    }
}


def get_state_dir():
    """Obtain the path to a directory to hold state.

    Returns a tuple of the path and a bool indicating whether the value came
    from an environment variable.
    """
    state_user_dir = os.path.expanduser('~/.mozbuild')
    state_env_dir = os.environ.get('MOZBUILD_STATE_PATH', None)

    if state_env_dir:
        return state_env_dir, True
    else:
        return state_user_dir, False


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
    try:
        import mach.main
    except ImportError:
        sys.path[0:0] = [os.path.join(mozilla_dir, path) for path in SEARCH_PATHS]
        import mach.main

    def pre_dispatch_handler(context, handler, args):
        """Perform global checks before command dispatch.

        Currently, our goal is to ensure developers periodically run
        `mach mercurial-setup` (when applicable) to ensure their Mercurial
        tools are up to date.
        """
        # Don't do anything when...

        # The user is performing a maintenance command.
        if handler.name in ('bootstrap', 'doctor', 'mercurial-setup'):
            return

        # We are running in automation.
        if 'MOZ_AUTOMATION' in os.environ:
            return

        # We are a curmudgeon who has found this undocumented variable.
        if 'I_PREFER_A_SUBOPTIMAL_MERCURIAL_EXPERIENCE' in os.environ:
            return

        # The environment is likely a machine invocation.
        if not sys.stdin.isatty():
            return

        # Mercurial isn't managing this source checkout.
        if not os.path.exists(os.path.join(topsrcdir, '.hg')):
            return

        state_dir = get_state_dir()[0]
        last_check_path = os.path.join(state_dir, 'mercurial',
                                       'setup.lastcheck')

        mtime = None
        try:
            mtime = os.path.getmtime(last_check_path)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

        # No last run file means mercurial-setup has never completed.
        if mtime is None:
            print(NO_MERCURIAL_SETUP, file=sys.stderr)
            sys.exit(2)
        elif time.time() - mtime > MERCURIAL_SETUP_FATAL_INTERVAL:
            print(OLD_MERCURIAL_TOOLS, file=sys.stderr)
            sys.exit(2)

    def populate_context(context, key=None):
        if key is None:
            return
        if key == 'state_dir':
            state_dir, is_environ = get_state_dir()
            if is_environ:
                if not os.path.exists(state_dir):
                    print('Creating global state directory from environment variable: %s'
                        % state_dir)
                    os.makedirs(state_dir, mode=0o770)
                    print('Please re-run mach.')
                    sys.exit(1)
            else:
                if not os.path.exists(state_dir):
                    print(STATE_DIR_FIRST_RUN.format(userdir=state_dir))
                    try:
                        for i in range(20, -1, -1):
                            time.sleep(1)
                            sys.stdout.write('%d ' % i)
                            sys.stdout.flush()
                    except KeyboardInterrupt:
                        sys.exit(1)

                    print('\nCreating default state directory: %s' % state_dir)
                    os.mkdir(state_dir)
                    print('Please re-run mach.')
                    sys.exit(1)

            return state_dir

        if key == 'topdir':
            return topsrcdir

        if key == 'pre_dispatch_handler':
            return pre_dispatch_handler

        raise AttributeError(key)

    mach = mach.main.Mach(os.getcwd())
    mach.populate_context_handler = populate_context

    for category, meta in CATEGORIES.items():
        mach.define_category(category, meta['short'], meta['long'],
            meta['priority'])

    for path in MACH_MODULES:
        mach.load_commands_from_file(os.path.join(mozilla_dir, path))

    return mach
