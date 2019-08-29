# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import re
import sys
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

        if args.suite != 'jstestbrowser' and not args.tests:
            test_subdir = {
                "reftest": os.path.join('layout', 'reftests'),
                "crashtest": os.path.join('layout', 'crashtest'),
            }[args.suite]
            args.tests = [test_subdir]

        tests = os.path.join(self.reftest_dir, 'tests')
        if not os.path.isdir(tests):
            os.symlink(self.topsrcdir, tests)

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
        args.sandboxReadWhitelist.extend([self.topsrcdir, self.topobjdir])

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
        """Runs a reftest, in an Android application."""

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
            args.app = "org.mozilla.geckoview.test"
        if not args.utilityPath:
            args.utilityPath = args.xrePath
        args.ignoreWindowSize = True
        args.printDeviceInfo = False

        from mozrunner.devices.android_device import get_adb_path

        if not args.adb_path:
            args.adb_path = get_adb_path(self)

        if 'geckoview' not in args.app:
            args.e10s = False
            print("using e10s=False for non-geckoview app")

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
    import reftestcommandline

    global parser
    here = os.path.abspath(os.path.dirname(__file__))
    build_obj = MozbuildObject.from_environment(cwd=here)
    if conditions.is_android(build_obj):
        parser = reftestcommandline.RemoteArgumentsParser()
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
        kwargs["topsrcdir"] = self.topsrcdir
        process_test_objects(kwargs)
        reftest = self._spawn(ReftestRunner)
        # Unstructured logging must be enabled prior to calling
        # adb which uses an unstructured logger in its constructor.
        reftest.log_manager.enable_unstructured()
        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            install = not kwargs.get('no_install')
            verify_android_device(self, install=install, xre=True, network=True,
                                  app=kwargs["app"], device_serial=kwargs["deviceSerial"])
            return reftest.run_android_test(**kwargs)
        return reftest.run_desktop_test(**kwargs)
