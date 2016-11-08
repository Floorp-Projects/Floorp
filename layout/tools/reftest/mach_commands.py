# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import re
import sys
import warnings
import which
from argparse import Namespace

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)

from mach.decorators import (
    CommandProvider,
    Command,
)

import reftestcommandline

ADB_NOT_FOUND = '''
The %s command requires the adb binary to be on your path.

If you have a B2G build, this can be found in
'%s/out/host/<platform>/bin'.
'''.lstrip()

GAIA_PROFILE_NOT_FOUND = '''
The reftest command requires a non-debug gaia profile on Mulet.
Either pass in --profile, or set the GAIA_PROFILE environment variable.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

GAIA_PROFILE_IS_DEBUG = '''
The reftest command requires a non-debug gaia profile on Mulet.
The specified profile, %s, is a debug profile.

If you do not have a non-debug gaia profile, you can build one:
    $ git clone https://github.com/mozilla-b2g/gaia
    $ cd gaia
    $ make

The profile should be generated in a directory called 'profile'.
'''.lstrip()

MARIONETTE_DISABLED = '''
The reftest command requires a marionette enabled build on Mulet.

Add 'ENABLE_MARIONETTE=1' to your mozconfig file and re-build the application.
Your currently active mozconfig is %s.
'''.lstrip()

parser = None


class ReftestRunner(MozbuildObject):
    """Easily run reftests.

    This currently contains just the basics for running reftests. We may want
    to hook up result parsing, etc.
    """
    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)

        # TODO Bug 794506 remove once mach integrates with virtualenv.
        build_path = os.path.join(self.topobjdir, 'build')
        if build_path not in sys.path:
            sys.path.append(build_path)

        self.tests_dir = os.path.join(self.topobjdir, '_tests')
        self.reftest_dir = os.path.join(self.tests_dir, 'reftest')

    def _make_shell_string(self, s):
        return "'%s'" % re.sub("'", r"'\''", s)

    def _setup_objdir(self, args):
        # reftest imports will happen from the objdir
        sys.path.insert(0, self.reftest_dir)

        if not args.tests:
            test_subdir = {
                "reftest": os.path.join('layout', 'reftests'),
                "crashtest": os.path.join('layout', 'crashtest'),
            }[args.suite]
            args.tests = [test_subdir]

        tests = os.path.join(self.reftest_dir, 'tests')
        if not os.path.isdir(tests):
            os.symlink(self.topsrcdir, tests)

    def run_b2g_test(self, b2g_home=None, xre_path=None, **kwargs):
        """Runs a b2g reftest.

        filter is a regular expression (in JS syntax, as could be passed to the
        RegExp constructor) to select which reftests to run from the manifest.

        tests is a list of paths. It can be a relative path from the
        top source directory, an absolute filename, or a directory containing
        test files.

        suite is the type of reftest to run. It can be one of ('reftest',
        'crashtest').
        """
        args = Namespace(**kwargs)
        if args.suite not in ('reftest', 'crashtest'):
            raise Exception('None or unrecognized reftest suite type.')

        self._setup_objdir(args)
        import runreftestb2g

        for i, path in enumerate(args.tests):
            # Non-absolute paths are relative to the packaged directory, which
            # has an extra tests/ at the start
            if os.path.exists(os.path.abspath(path)):
                path = os.path.relpath(path, os.path.join(self.topsrcdir))
            args.tests[i] = os.path.join('tests', path)

        try:
            which.which('adb')
        except which.WhichError:
            # TODO Find adb automatically if it isn't on the path
            raise Exception(ADB_NOT_FOUND % ('%s-remote' % args.suite, b2g_home))

        args.b2gPath = b2g_home
        args.logdir = self.reftest_dir
        args.httpdPath = os.path.join(self.topsrcdir, 'netwerk', 'test', 'httpserver')
        args.xrePath = xre_path
        args.ignoreWindowSize = True

        return runreftestb2g.run_test_harness(parser, args)

    def run_mulet_test(self, **kwargs):
        """Runs a mulet reftest."""
        args = Namespace(**kwargs)
        self._setup_objdir(args)

        import runreftestmulet

        if self.substs.get('ENABLE_MARIONETTE') != '1':
            print(MARIONETTE_DISABLED % self.mozconfig['path'])
            return 1

        if not args.profile:
            gaia_profile = os.environ.get('GAIA_PROFILE')
            if not gaia_profile:
                print(GAIA_PROFILE_NOT_FOUND)
                return 1
            args.profile = gaia_profile

        if os.path.isfile(os.path.join(args.profile, 'extensions',
                                       'httpd@gaiamobile.org')):
            print(GAIA_PROFILE_IS_DEBUG % args.profile)
            return 1

        args.app = self.get_binary_path()
        args.mulet = True

        if not args.app.endswith('-bin'):
            args.app = '%s-bin' % args.app
        if not os.path.isfile(args.app):
            args.app = args.app[:-len('-bin')]

        return runreftestmulet.run_test_harness(parser, args)

    def run_desktop_test(self, **kwargs):
        """Runs a reftest, in desktop Firefox."""
        import runreftest

        args = Namespace(**kwargs)
        if args.suite not in ('reftest', 'crashtest', 'jstestbrowser'):
            raise Exception('None or unrecognized reftest suite type.')

        default_manifest = {
            "reftest": (self.topsrcdir, "layout", "reftests", "reftest.list"),
            "crashtest": (self.topsrcdir, "testing", "crashtest", "crashtests.list"),
            "jstestbrowser": (self.topobjdir, "dist", "test-stage", "jsreftest", "tests",
                              "jstests.list")
        }

        args.extraProfileFiles.append(os.path.join(self.topobjdir, "dist", "plugins"))
        args.symbolsPath = os.path.join(self.topobjdir, "crashreporter-symbols")

        if not args.tests:
            args.tests = [os.path.join(*default_manifest[args.suite])]

        if args.suite == "jstestbrowser":
            args.extraProfileFiles.append(os.path.join(self.topobjdir, "dist",
                                                            "test-stage", "jsreftest",
                                                            "tests", "user.js"))

        self.log_manager.enable_unstructured()
        try:
            rv = runreftest.run_test_harness(parser, args)
        finally:
            self.log_manager.disable_unstructured()

        return rv

    def run_android_test(self, **kwargs):
        """Runs a reftest, in Firefox for Android."""

        args = Namespace(**kwargs)
        if args.suite not in ('reftest', 'crashtest', 'jstestbrowser'):
            raise Exception('None or unrecognized reftest suite type.')

        self._setup_objdir(args)
        import remotereftest

        default_manifest = {
            "reftest": (self.topsrcdir, "layout", "reftests", "reftest.list"),
            "crashtest": (self.topsrcdir, "testing", "crashtest", "crashtests.list"),
            "jstestbrowser": ("jsreftest", "tests", "jstests.list")
        }

        if not args.tests:
            args.tests = [os.path.join(*default_manifest[args.suite])]

        args.extraProfileFiles.append(
            os.path.join(self.topsrcdir, "mobile", "android", "fonts"))

        hyphenation_path = os.path.join(self.topsrcdir, "intl", "locales")

        for (dirpath, dirnames, filenames) in os.walk(hyphenation_path):
            for filename in filenames:
                if filename.endswith('.dic'):
                    args.extraProfileFiles.append(os.path.join(dirpath, filename))

        if not args.httpdPath:
            args.httpdPath = os.path.join(self.tests_dir, "modules")
        if not args.symbolsPath:
            args.symbolsPath = os.path.join(self.topobjdir, "crashreporter-symbols")
        if not args.xrePath:
            args.xrePath = os.environ.get("MOZ_HOST_BIN")
        if not args.app:
            args.app = self.substs["ANDROID_PACKAGE_NAME"]
        if not args.utilityPath:
            args.utilityPath = args.xrePath
        args.dm_trans = "adb"
        args.ignoreWindowSize = True
        args.printDeviceInfo = False

        from mozrunner.devices.android_device import grant_runtime_permissions
        grant_runtime_permissions(self)

        # A symlink and some path manipulations are required so that test
        # manifests can be found both locally and remotely (via a url)
        # using the same relative path.
        if args.suite == "jstestbrowser":
            staged_js_dir = os.path.join(self.topobjdir, "dist", "test-stage", "jsreftest")
            tests = os.path.join(self.reftest_dir, 'jsreftest')
            if not os.path.isdir(tests):
                os.symlink(staged_js_dir, tests)
            args.extraProfileFiles.append(os.path.join(staged_js_dir, "tests", "user.js"))
        else:
            tests = os.path.join(self.reftest_dir, "tests")
            if not os.path.isdir(tests):
                os.symlink(self.topsrcdir, tests)
            for i, path in enumerate(args.tests):
                # Non-absolute paths are relative to the packaged directory, which
                # has an extra tests/ at the start
                if os.path.exists(os.path.abspath(path)):
                    path = os.path.relpath(path, os.path.join(self.topsrcdir))
                args.tests[i] = os.path.join('tests', path)

        self.log_manager.enable_unstructured()
        try:
            rv = remotereftest.run_test_harness(parser, args)
        finally:
            self.log_manager.disable_unstructured()

        return rv


def process_test_objects(kwargs):
    """|mach test| works by providing a test_objects argument, from
    which the test path must be extracted and converted into a normal
    reftest tests argument."""

    if "test_objects" in kwargs:
        if kwargs["tests"] is None:
            kwargs["tests"] = []
        kwargs["tests"].extend(item["path"] for item in kwargs["test_objects"])
        del kwargs["test_objects"]


def get_parser():
    global parser
    here = os.path.abspath(os.path.dirname(__file__))
    build_obj = MozbuildObject.from_environment(cwd=here)
    if conditions.is_android(build_obj):
        parser = reftestcommandline.RemoteArgumentsParser()
    elif conditions.is_mulet(build_obj):
        parser = reftestcommandline.B2GArgumentParser()
    else:
        parser = reftestcommandline.DesktopArgumentsParser()
    return parser


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('reftest',
             category='testing',
             description='Run reftests (layout and graphics correctness).',
             parser=get_parser)
    def run_reftest(self, **kwargs):
        kwargs["suite"] = "reftest"
        return self._run_reftest(**kwargs)

    @Command('jstestbrowser',
             category='testing',
             description='Run js/src/tests in the browser.',
             parser=get_parser)
    def run_jstestbrowser(self, **kwargs):
        self._mach_context.commands.dispatch("build",
                                             self._mach_context,
                                             what=["stage-jstests"])
        kwargs["suite"] = "jstestbrowser"
        return self._run_reftest(**kwargs)

    @Command('crashtest',
             category='testing',
             description='Run crashtests (Check if crashes on a page).',
             parser=get_parser)
    def run_crashtest(self, **kwargs):
        kwargs["suite"] = "crashtest"
        return self._run_reftest(**kwargs)

    def _run_reftest(self, **kwargs):
        process_test_objects(kwargs)
        reftest = self._spawn(ReftestRunner)
        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self, install=True, xre=True)
            return reftest.run_android_test(**kwargs)
        elif conditions.is_mulet(self):
            return reftest.run_mulet_test(**kwargs)
        return reftest.run_desktop_test(**kwargs)


# TODO For now b2g commands will only work with the emulator,
# they should be modified to work with all devices.
def is_emulator(cls):
    """Emulator needs to be configured."""
    return cls.device_name.startswith('emulator')


@CommandProvider
class B2GCommands(MachCommandBase):
    def __init__(self, context):
        MachCommandBase.__init__(self, context)

        for attr in ('b2g_home', 'xre_path', 'device_name'):
            setattr(self, attr, getattr(context, attr, None))

    @Command('reftest-remote', category='testing',
             description='Run a remote reftest (b2g layout and graphics correctness, remote device).',
             conditions=[conditions.is_b2g, is_emulator],
             parser=get_parser)
    def run_reftest_remote(self, **kwargs):
        kwargs["suite"] = "reftest"
        return self._run_reftest(**kwargs)

    @Command('crashtest-remote', category='testing',
             description='Run a remote crashtest (Check if b2g crashes on a page, remote device).',
             conditions=[conditions.is_b2g, is_emulator],
             parser=get_parser)
    def run_crashtest_remote(self, test_file, **kwargs):
        kwargs["suite"] = "crashtest"
        return self._run_reftest(**kwargs)

    def _run_reftest(self, **kwargs):
        process_test_objects(kwargs)
        if self.device_name:
            if self.device_name.startswith('emulator'):
                emulator = 'arm'
                if 'x86' in self.device_name:
                    emulator = 'x86'
                kwargs['emulator'] = emulator

        reftest = self._spawn(ReftestRunner)
        return reftest.run_b2g_test(self.b2g_home, self.xre_path, **kwargs)
