# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
from argparse import Namespace
from functools import partial

from mach.decorators import (
    CommandProvider,
    Command,
)

here = os.path.abspath(os.path.dirname(__file__))


def run_reftest(context, **kwargs):
    import mozinfo

    args = Namespace(**kwargs)
    args.e10s = context.mozharness_config.get('e10s', args.e10s)

    if not args.tests:
        args.tests = [os.path.join('layout', 'reftests', 'reftest.list')]

    test_root = os.path.join(context.package_root, 'reftest', 'tests')
    normalize = partial(context.normalize_test_path, test_root)
    args.tests = map(normalize, args.tests)

    if mozinfo.info.get('buildapp') == 'mobile/android':
        return run_reftest_android(context, args)
    return run_reftest_desktop(context, args)


def run_reftest_desktop(context, args):
    from runreftest import run_test_harness

    args.app = args.app or context.firefox_bin
    args.extraProfileFiles.append(os.path.join(context.bin_dir, 'plugins'))
    args.utilityPath = context.bin_dir

    return run_test_harness(parser, args)


def run_reftest_android(context, args):
    from remotereftest import run_test_harness

    args.app = args.app or 'org.mozilla.fennec'
    args.utilityPath = context.hostutils
    args.xrePath = context.hostutils
    args.httpdPath = context.module_dir
    args.ignoreWindowSize = True
    args.printDeviceInfo = False

    config = context.mozharness_config
    if config:
        args.remoteWebServer = config['remote_webserver']
        args.httpPort = config['emulator']['http_port']
        args.sslPort = config['emulator']['ssl_port']
        args.adb_path = config['exes']['adb'] % {'abs_work_dir': context.mozharness_workdir}

    return run_test_harness(parser, args)


def setup_argument_parser():
    import mozinfo
    import reftestcommandline

    global parser
    mozinfo.find_and_update_from_json(here)
    if mozinfo.info.get('buildapp') == 'mobile/android':
        parser = reftestcommandline.RemoteArgumentsParser()
    else:
        parser = reftestcommandline.DesktopArgumentsParser()
    return parser


@CommandProvider
class ReftestCommands(object):

    def __init__(self, context):
        self.context = context

    @Command('reftest', category='testing',
             description='Run the reftest harness.',
             parser=setup_argument_parser)
    def reftest(self, **kwargs):
        self.context.activate_mozharness_venv()
        kwargs['suite'] = 'reftest'
        return run_reftest(self.context, **kwargs)
