# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains code for populating the virtualenv environment for
# Mozilla's build system. It is typically called as part of configure.

from __future__ import print_function, unicode_literals

import distutils.sysconfig
import os
import shutil
import subprocess
import sys

from distutils.version import StrictVersion


# Minimum version of Python required to build.
MINIMUM_PYTHON_VERSION = StrictVersion('2.7.3')
MINIMUM_PYTHON_MAJOR = 2


UPGRADE_WINDOWS = '''
Please upgrade to the latest MozillaBuild development environments. See
https://developer.mozilla.org/en-US/docs/Developer_Guide/Build_Instructions/Windows_Prerequisites
'''.lstrip()

UPGRADE_OTHER = '''
Run |mach bootstrap| to ensure your system is up to date.

If you still receive this error, your shell environment is likely detecting
another Python version. Ensure a modern Python can be found in the paths
defined by the $PATH environment variable and try again.
'''.lstrip()


class VirtualenvManager(object):
    """Contains logic for managing virtualenvs for building the tree."""

    def __init__(self, topsrcdir, topobjdir, virtualenv_path, log_handle,
        manifest_path):
        """Create a new manager.

        Each manager is associated with a source directory, a path where you
        want the virtualenv to be created, and a handle to write output to.
        """
        assert os.path.isabs(manifest_path), "manifest_path must be an absolute path: %s" % (manifest_path)
        self.topsrcdir = topsrcdir
        self.topobjdir = topobjdir
        self.virtualenv_root = virtualenv_path
        self.log_handle = log_handle
        self.manifest_path = manifest_path

    @property
    def virtualenv_script_path(self):
        """Path to virtualenv's own populator script."""
        return os.path.join(self.topsrcdir, 'python', 'virtualenv',
            'virtualenv.py')

    @property
    def python_path(self):
        if sys.platform in ('win32', 'cygwin'):
            return os.path.join(self.virtualenv_root, 'Scripts', 'python.exe')

        return os.path.join(self.virtualenv_root, 'bin', 'python')

    @property
    def activate_path(self):
        if sys.platform in ('win32', 'cygwin'):
            return os.path.join(self.virtualenv_root, 'Scripts',
                'activate_this.py')

        return os.path.join(self.virtualenv_root, 'bin', 'activate_this.py')

    def up_to_date(self):
        """Returns whether the virtualenv is present and up to date."""

        deps = [self.manifest_path, __file__]

        # check if virtualenv exists
        if not os.path.exists(self.virtualenv_root) or \
            not os.path.exists(self.activate_path):

            return False

        # check modification times
        activate_mtime = os.path.getmtime(self.activate_path)
        dep_mtime = max(os.path.getmtime(p) for p in deps)
        if dep_mtime > activate_mtime:
            return False

        # recursively check sub packages.txt files
        submanifests = [i[1] for i in self.packages()
                        if i[0] == 'packages.txt']
        for submanifest in submanifests:
            submanifest = os.path.join(self.topsrcdir, submanifest)
            submanager = VirtualenvManager(self.topsrcdir,
                                           self.topobjdir,
                                           self.virtualenv_root,
                                           self.log_handle,
                                           submanifest)
            if not submanager.up_to_date():
                return False

        return True

    def ensure(self):
        """Ensure the virtualenv is present and up to date.

        If the virtualenv is up to date, this does nothing. Otherwise, it
        creates and populates the virtualenv as necessary.

        This should be the main API used from this class as it is the
        highest-level.
        """
        if self.up_to_date():
            return self.virtualenv_root
        return self.build()

    def create(self):
        """Create a new, empty virtualenv.

        Receives the path to virtualenv's virtualenv.py script (which will be
        called out to), the path to create the virtualenv in, and a handle to
        write output to.
        """
        env = dict(os.environ)
        env.pop('PYTHONDONTWRITEBYTECODE', None)

        args = [sys.executable, self.virtualenv_script_path,
            '--system-site-packages', self.virtualenv_root]

        result = subprocess.call(args, stdout=self.log_handle,
            stderr=subprocess.STDOUT, env=env)

        if result:
            raise Exception('Error creating virtualenv.')

        return self.virtualenv_root

    def packages(self):
        with file(self.manifest_path, 'rU') as fh:
            packages = [line.rstrip().split(':')
                        for line in fh]
        return packages

    def populate(self):
        """Populate the virtualenv.

        The manifest file consists of colon-delimited fields. The first field
        specifies the action. The remaining fields are arguments to that
        action. The following actions are supported:

        setup.py -- Invoke setup.py for a package. Expects the arguments:
            1. relative path directory containing setup.py.
            2. argument(s) to setup.py. e.g. "develop". Each program argument
               is delimited by a colon. Arguments with colons are not yet
               supported.

        filename.pth -- Adds the path given as argument to filename.pth under
            the virtualenv site packages directory.

        optional -- This denotes the action as optional. The requested action
            is attempted. If it fails, we issue a warning and go on. The
            initial "optional" field is stripped then the remaining line is
            processed like normal. e.g.
            "optional:setup.py:python/foo:built_ext:-i"

        copy -- Copies the given file in the virtualenv site packages
            directory.

        packages.txt -- Denotes that the specified path is a child manifest. It
            will be read and processed as if its contents were concatenated
            into the manifest being read.

        objdir -- Denotes a relative path in the object directory to add to the
            search path. e.g. "objdir:build" will add $topobjdir/build to the
            search path.

        Note that the Python interpreter running this function should be the
        one from the virtualenv. If it is the system Python or if the
        environment is not configured properly, packages could be installed
        into the wrong place. This is how virtualenv's work.
        """

        packages = self.packages()

        def handle_package(package):
            python_lib = distutils.sysconfig.get_python_lib()
            if package[0] == 'setup.py':
                assert len(package) >= 2

                self.call_setup(os.path.join(self.topsrcdir, package[1]),
                    package[2:])

                return True

            if package[0] == 'copy':
                assert len(package) == 2

                src = os.path.join(self.topsrcdir, package[1])
                dst = os.path.join(python_lib, os.path.basename(package[1]))

                shutil.copy(src, dst)

                return True

            if package[0] == 'packages.txt':
                assert len(package) == 2

                src = os.path.join(self.topsrcdir, package[1])
                assert os.path.isfile(src), "'%s' does not exist" % src
                submanager = VirtualenvManager(self.topsrcdir,
                                               self.topobjdir,
                                               self.virtualenv_root,
                                               self.log_handle,
                                               src)
                submanager.populate()

                return True

            if package[0].endswith('.pth'):
                assert len(package) == 2

                path = os.path.join(self.topsrcdir, package[1])

                with open(os.path.join(python_lib, package[0]), 'a') as f:
                    # This path is relative to the .pth file.  Using a
                    # relative path allows the srcdir/objdir combination
                    # to be moved around (as long as the paths relative to
                    # each other remain the same).
                    f.write("%s\n" % os.path.relpath(path, python_lib))

                return True

            if package[0] == 'optional':
                try:
                    handle_package(package[1:])
                    return True
                except:
                    print('Error processing command. Ignoring', \
                        'because optional. (%s)' % ':'.join(package),
                        file=self.log_handle)
                    return False

            if package[0] == 'objdir':
                assert len(package) == 2
                path = os.path.join(self.topobjdir, package[1])

                with open(os.path.join(python_lib, 'objdir.pth'), 'a') as f:
                    f.write('%s\n' % path)

                return True

            raise Exception('Unknown action: %s' % package[0])

        # We always target the OS X deployment target that Python itself was
        # built with, regardless of what's in the current environment. If we
        # don't do # this, we may run into a Python bug. See
        # http://bugs.python.org/issue9516 and bug 659881.
        #
        # Note that this assumes that nothing compiled in the virtualenv is
        # shipped as part of a distribution. If we do ship anything, the
        # deployment target here may be different from what's targeted by the
        # shipping binaries and # virtualenv-produced binaries may fail to
        # work.
        #
        # We also ignore environment variables that may have been altered by
        # configure or a mozconfig activated in the current shell. We trust
        # Python is smart enough to find a proper compiler and to use the
        # proper compiler flags. If it isn't your Python is likely broken.
        IGNORE_ENV_VARIABLES = ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS',
            'PYTHONDONTWRITEBYTECODE')

        try:
            old_target = os.environ.get('MACOSX_DEPLOYMENT_TARGET', None)
            sysconfig_target = \
                distutils.sysconfig.get_config_var('MACOSX_DEPLOYMENT_TARGET')

            if sysconfig_target is not None:
                os.environ['MACOSX_DEPLOYMENT_TARGET'] = sysconfig_target

            old_env_variables = {}
            for k in IGNORE_ENV_VARIABLES:
                if k not in os.environ:
                    continue

                old_env_variables[k] = os.environ[k]
                del os.environ[k]

            for package in packages:
                handle_package(package)
        finally:
            os.environ.pop('MACOSX_DEPLOYMENT_TARGET', None)

            if old_target is not None:
                os.environ['MACOSX_DEPLOYMENT_TARGET'] = old_target

            os.environ.update(old_env_variables)

    def call_setup(self, directory, arguments):
        """Calls setup.py in a directory."""
        setup = os.path.join(directory, 'setup.py')

        program = [sys.executable, setup]
        program.extend(arguments)

        # We probably could call the contents of this file inside the context
        # of this interpreter using execfile() or similar. However, if global
        # variables like sys.path are adjusted, this could cause all kinds of
        # havoc. While this may work, invoking a new process is safer.

        try:
            output = subprocess.check_output(program, cwd=directory, stderr=subprocess.STDOUT)
            print(output)
        except subprocess.CalledProcessError as e:
            if 'Python.h: No such file or directory' in e.output:
                print('WARNING: Python.h not found. Install Python development headers.')
            else:
                print(e.output)

            raise Exception('Error installing package: %s' % directory)

    def build(self):
        """Build a virtualenv per tree conventions.

        This returns the path of the created virtualenv.
        """

        self.create()

        # We need to populate the virtualenv using the Python executable in
        # the virtualenv for paths to be proper.

        args = [self.python_path, __file__, 'populate', self.topsrcdir,
            self.topobjdir, self.virtualenv_root]

        result = subprocess.call(args, stdout=self.log_handle,
            stderr=subprocess.STDOUT, cwd=self.topsrcdir)

        if result != 0:
            raise Exception('Error populating virtualenv.')

        os.utime(self.activate_path, None)

        return self.virtualenv_root

    def activate(self):
        """Activate the virtualenv in this Python context.

        If you run a random Python script and wish to "activate" the
        virtualenv, you can simply instantiate an instance of this class
        and call .ensure() and .activate() to make the virtualenv active.
        """

        execfile(self.activate_path, dict(__file__=self.activate_path))


def verify_python_version(log_handle):
    """Ensure the current version of Python is sufficient."""
    major, minor, micro = sys.version_info[:3]

    our = StrictVersion('%d.%d.%d' % (major, minor, micro))

    if major != MINIMUM_PYTHON_MAJOR or our < MINIMUM_PYTHON_VERSION:
        log_handle.write('Python %s or greater (but not Python 3) is '
            'required to build. ' % MINIMUM_PYTHON_VERSION)
        log_handle.write('You are running Python %s.\n' % our)

        if os.name in ('nt', 'ce'):
            log_handle.write(UPGRADE_WINDOWS)
        else:
            log_handle.write(UPGRADE_OTHER)

        sys.exit(1)


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print('Usage: populate_virtualenv.py /path/to/topsrcdir /path/to/topobjdir /path/to/virtualenv')
        sys.exit(1)

    verify_python_version(sys.stdout)

    topsrcdir = sys.argv[1]
    topobjdir = sys.argv[2]
    virtualenv_path = sys.argv[3]
    populate = False

    # This should only be called internally.
    if sys.argv[1] == 'populate':
        populate = True
        topsrcdir = sys.argv[2]
        topobjdir = sys.argv[3]
        virtualenv_path = sys.argv[4]

    # path to default packages.txt
    manifest_path = os.path.join(topsrcdir, 'build', 'virtualenv', 'packages.txt')

    manager = VirtualenvManager(topsrcdir, topobjdir, virtualenv_path,
        sys.stdout, manifest_path)

    if populate:
        manager.populate()
    else:
        manager.ensure()

