# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals, print_function

import os
import sys
from argparse import Namespace
from functools import partial

from mach.decorators import (
    CommandProvider,
    Command,
)
from mozbuild.base import MachCommandBase

here = os.path.abspath(os.path.dirname(__file__))
logger = None


def run_reftest(context, **kwargs):
    import mozinfo
    from mozlog.commandline import setup_logging

    if not kwargs.get("log"):
        kwargs["log"] = setup_logging("reftest", kwargs, {"mach": sys.stdout})
    global logger
    logger = kwargs["log"]

    args = Namespace(**kwargs)
    args.e10s = context.mozharness_config.get("e10s", args.e10s)

    if not args.tests:
        args.tests = [os.path.join("layout", "reftests", "reftest.list")]

    test_root = os.path.join(context.package_root, "reftest", "tests")
    normalize = partial(context.normalize_test_path, test_root)
    args.tests = map(normalize, args.tests)

    if kwargs.get("allow_software_gl_layers"):
        os.environ["MOZ_LAYERS_ALLOW_SOFTWARE_GL"] = "1"

    if mozinfo.info.get("buildapp") == "mobile/android":
        return run_reftest_android(context, args)
    return run_reftest_desktop(context, args)


def run_reftest_desktop(context, args):
    from runreftest import run_test_harness

    args.app = args.app or context.firefox_bin
    args.extraProfileFiles.append(os.path.join(context.bin_dir, "plugins"))
    args.utilityPath = context.bin_dir
    args.sandboxReadWhitelist.append(context.mozharness_workdir)
    args.extraPrefs.append("layers.acceleration.force-enabled=true")

    logger.info("mach calling runreftest with args: " + str(args))

    return run_test_harness(parser, args)


def run_reftest_android(context, args):
    from remotereftest import run_test_harness

    args.app = args.app or "org.mozilla.geckoview.test"
    args.utilityPath = context.hostutils
    args.xrePath = context.hostutils
    args.httpdPath = context.module_dir
    args.ignoreWindowSize = True

    config = context.mozharness_config
    if config:
        host = os.environ.get("HOST_IP", "10.0.2.2")
        args.remoteWebServer = config.get("remote_webserver", host)
        args.httpPort = config.get("http_port", 8854)
        args.sslPort = config.get("ssl_port", 4454)
        args.adb_path = config["exes"]["adb"] % {
            "abs_work_dir": context.mozharness_workdir
        }
        args.deviceSerial = os.environ.get("DEVICE_SERIAL", "emulator-5554")

    logger.info("mach calling remotereftest with args: " + str(args))

    return run_test_harness(parser, args)


def add_global_arguments(parser):
    parser.add_argument("--test-suite")
    parser.add_argument("--reftest-suite")
    parser.add_argument("--download-symbols")
    parser.add_argument("--allow-software-gl-layers", action="store_true")
    parser.add_argument("--no-run-tests", action="store_true")


def setup_argument_parser():
    import mozinfo
    import reftestcommandline

    global parser
    mozinfo.find_and_update_from_json(here)
    if mozinfo.info.get("buildapp") == "mobile/android":
        parser = reftestcommandline.RemoteArgumentsParser()
    else:
        parser = reftestcommandline.DesktopArgumentsParser()
    add_global_arguments(parser)
    return parser


@CommandProvider
class ReftestCommands(MachCommandBase):
    @Command(
        "reftest",
        category="testing",
        description="Run the reftest harness.",
        parser=setup_argument_parser,
    )
    def reftest(self, **kwargs):
        self._mach_context.activate_mozharness_venv()
        kwargs["suite"] = "reftest"
        return run_reftest(self._mach_context, **kwargs)
