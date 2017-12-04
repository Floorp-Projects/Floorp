# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import errno
import json
import os
import platform
import random
import subprocess
import sys
import uuid
import __builtin__

from types import ModuleType


STATE_DIR_FIRST_RUN = '''
mach and the build system store shared state in a common directory on the
filesystem. The following directory will be created:

  {userdir}

If you would like to use a different directory, hit CTRL+c and set the
MOZBUILD_STATE_PATH environment variable to the directory you would like to
use and re-run mach. For this change to take effect forever, you'll likely
want to export this environment variable from your shell's init scripts.

Press ENTER/RETURN to continue or CTRL+c to abort.
'''.lstrip()


# Individual files providing mach commands.
MACH_MODULES = [
    'build/valgrind/mach_commands.py',
    'devtools/shared/css/generated/mach_commands.py',
    'dom/bindings/mach_commands.py',
    'layout/tools/reftest/mach_commands.py',
    'python/mach_commands.py',
    'python/mach/mach/commands/commandinfo.py',
    'python/mach/mach/commands/settings.py',
    'python/mozboot/mozboot/mach_commands.py',
    'python/mozbuild/mozbuild/mach_commands.py',
    'python/mozbuild/mozbuild/backend/mach_commands.py',
    'python/mozbuild/mozbuild/compilation/codecomplete.py',
    'python/mozbuild/mozbuild/frontend/mach_commands.py',
    'taskcluster/mach_commands.py',
    'testing/awsy/mach_commands.py',
    'testing/firefox-ui/mach_commands.py',
    'testing/geckodriver/mach_commands.py',
    'testing/mach_commands.py',
    'testing/marionette/mach_commands.py',
    'testing/mochitest/mach_commands.py',
    'testing/mozharness/mach_commands.py',
    'testing/talos/mach_commands.py',
    'testing/web-platform/mach_commands.py',
    'testing/xpcshell/mach_commands.py',
    'tools/compare-locales/mach_commands.py',
    'tools/docs/mach_commands.py',
    'tools/lint/mach_commands.py',
    'tools/mach_commands.py',
    'tools/power/mach_commands.py',
    'tools/tryselect/mach_commands.py',
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
        'long': 'The disabled commands are hidden by default. Use -v to display them. '
        'These commands are unavailable for your current context, '
        'run "mach <command>" to see why.',
        'priority': 0,
    }
}


# We submit data to telemetry approximately every this many mach invocations
TELEMETRY_SUBMISSION_FREQUENCY = 10


def search_path(mozilla_dir, packages_txt):
    with open(os.path.join(mozilla_dir, packages_txt)) as f:
        packages = [line.rstrip().split(':') for line in f]

    def handle_package(package):
        if package[0] == 'optional':
            try:
                for path in handle_package(package[1:]):
                    yield path
            except:
                pass

        if package[0] == 'packages.txt':
            assert len(package) == 2
            for p in search_path(mozilla_dir, package[1]):
                yield os.path.join(mozilla_dir, p)

        if package[0].endswith('.pth'):
            assert len(package) == 2
            yield os.path.join(mozilla_dir, package[1])

    for package in packages:
        for path in handle_package(package):
            yield path


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
    sys.path[0:0] = [os.path.join(mozilla_dir, path)
                     for path in search_path(mozilla_dir,
                                             'build/virtualenv_packages.txt')]
    import mach.base
    import mach.main
    from mozboot.util import get_state_dir

    from mozbuild.util import patch_main
    patch_main()

    def resolve_repository():
        import mozversioncontrol

        try:
            # This API doesn't respect the vcs binary choices from configure.
            # If we ever need to use the VCS binary here, consider something
            # more robust.
            return mozversioncontrol.get_repository_object(path=mozilla_dir)
        except (mozversioncontrol.InvalidRepoPath,
                mozversioncontrol.MissingVCSTool):
            return None

    def telemetry_handler(context, data):
        # We have not opted-in to telemetry
        if 'BUILD_SYSTEM_TELEMETRY' not in os.environ:
            return

        telemetry_dir = os.path.join(get_state_dir()[0], 'telemetry')
        try:
            os.mkdir(telemetry_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise
        outgoing_dir = os.path.join(telemetry_dir, 'outgoing')
        try:
            os.mkdir(outgoing_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

        # Add common metadata to help submit sorted data later on.
        data['argv'] = sys.argv
        data.setdefault('system', {}).update(dict(
            architecture=list(platform.architecture()),
            machine=platform.machine(),
            python_version=platform.python_version(),
            release=platform.release(),
            system=platform.system(),
            version=platform.version(),
        ))

        if platform.system() == 'Linux':
            dist = list(platform.linux_distribution())
            data['system']['linux_distribution'] = dist
        elif platform.system() == 'Windows':
            win32_ver = list((platform.win32_ver())),
            data['system']['win32_ver'] = win32_ver
        elif platform.system() == 'Darwin':
            # mac version is a special Cupertino snowflake
            r, v, m = platform.mac_ver()
            data['system']['mac_ver'] = [r, list(v), m]

        with open(os.path.join(outgoing_dir, str(uuid.uuid4()) + '.json'),
                  'w') as f:
            json.dump(data, f, sort_keys=True)

    def should_skip_dispatch(context, handler):
        # The user is performing a maintenance command.
        if handler.name in ('bootstrap', 'doctor', 'mach-commands', 'mercurial-setup'):
            return True

        # We are running in automation.
        if 'MOZ_AUTOMATION' in os.environ or 'TASK_ID' in os.environ:
            return True

        # The environment is likely a machine invocation.
        if sys.stdin.closed or not sys.stdin.isatty():
            return True

        return False

    def post_dispatch_handler(context, handler, args):
        """Perform global operations after command dispatch.


        For now,  we will use this to handle build system telemetry.
        """
        # Don't do anything when...
        if should_skip_dispatch(context, handler):
            return

        # We call mach environment in client.mk which would cause the
        # data submission below to block the forward progress of make.
        if handler.name in ('environment'):
            return

        # We have not opted-in to telemetry
        if 'BUILD_SYSTEM_TELEMETRY' not in os.environ:
            return

        # Every n-th operation
        if random.randint(1, TELEMETRY_SUBMISSION_FREQUENCY) != 1:
            return

        with open(os.devnull, 'wb') as devnull:
            subprocess.Popen([sys.executable,
                              os.path.join(topsrcdir, 'build',
                                           'submit_telemetry_data.py'),
                              get_state_dir()[0]],
                             stdout=devnull, stderr=devnull)

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
            else:
                if not os.path.exists(state_dir):
                    if not os.environ.get('MOZ_AUTOMATION'):
                        print(STATE_DIR_FIRST_RUN.format(userdir=state_dir))
                        try:
                            sys.stdin.readline()
                        except KeyboardInterrupt:
                            sys.exit(1)

                    print('\nCreating default state directory: %s' % state_dir)
                    os.makedirs(state_dir, mode=0o770)

            return state_dir

        if key == 'topdir':
            return topsrcdir

        if key == 'telemetry_handler':
            return telemetry_handler

        if key == 'post_dispatch_handler':
            return post_dispatch_handler

        if key == 'repository':
            return resolve_repository()

        raise AttributeError(key)

    driver = mach.main.Mach(os.getcwd())
    driver.populate_context_handler = populate_context

    if not driver.settings_paths:
        # default global machrc location
        driver.settings_paths.append(get_state_dir()[0])
    # always load local repository configuration
    driver.settings_paths.append(mozilla_dir)

    for category, meta in CATEGORIES.items():
        driver.define_category(category, meta['short'], meta['long'],
                               meta['priority'])

    repo = resolve_repository()

    for path in MACH_MODULES:
        # Sparse checkouts may not have all mach_commands.py files. Ignore
        # errors from missing files.
        try:
            driver.load_commands_from_file(os.path.join(mozilla_dir, path))
        except mach.base.MissingFileError:
            if not repo or not repo.sparse_checkout_present():
                raise

    return driver


# Hook import such that .pyc/.pyo files without a corresponding .py file in
# the source directory are essentially ignored. See further below for details
# and caveats.
# Objdirs outside the source directory are ignored because in most cases, if
# a .pyc/.pyo file exists there, a .py file will be next to it anyways.
class ImportHook(object):
    def __init__(self, original_import):
        self._original_import = original_import
        # Assume the source directory is the parent directory of the one
        # containing this file.
        self._source_dir = os.path.normcase(os.path.abspath(
            os.path.dirname(os.path.dirname(__file__)))) + os.sep
        self._modules = set()

    def __call__(self, name, globals=None, locals=None, fromlist=None,
                 level=-1):
        # name might be a relative import. Instead of figuring out what that
        # resolves to, which is complex, just rely on the real import.
        # Since we don't know the full module name, we can't check sys.modules,
        # so we need to keep track of which modules we've already seen to avoid
        # to stat() them again when they are imported multiple times.
        module = self._original_import(name, globals, locals, fromlist, level)

        # Some tests replace modules in sys.modules with non-module instances.
        if not isinstance(module, ModuleType):
            return module

        resolved_name = module.__name__
        if resolved_name in self._modules:
            return module
        self._modules.add(resolved_name)

        # Builtin modules don't have a __file__ attribute.
        if not hasattr(module, '__file__'):
            return module

        # Note: module.__file__ is not always absolute.
        path = os.path.normcase(os.path.abspath(module.__file__))
        # Note: we could avoid normcase and abspath above for non pyc/pyo
        # files, but those are actually rare, so it doesn't really matter.
        if not path.endswith(('.pyc', '.pyo')):
            return module

        # Ignore modules outside our source directory
        if not path.startswith(self._source_dir):
            return module

        # If there is no .py corresponding to the .pyc/.pyo module we're
        # loading, remove the .pyc/.pyo file, and reload the module.
        # Since we already loaded the .pyc/.pyo module, if it had side
        # effects, they will have happened already, and loading the module
        # with the same name, from another directory may have the same side
        # effects (or different ones). We assume it's not a problem for the
        # python modules under our source directory (either because it
        # doesn't happen or because it doesn't matter).
        if not os.path.exists(module.__file__[:-1]):
            if os.path.exists(module.__file__):
                os.remove(module.__file__)
            del sys.modules[module.__name__]
            module = self(name, globals, locals, fromlist, level)

        return module


# Install our hook
__builtin__.__import__ = ImportHook(__builtin__.__import__)
