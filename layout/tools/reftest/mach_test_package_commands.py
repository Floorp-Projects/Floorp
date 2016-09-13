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


def run_reftest(context, **kwargs):
    import mozinfo

    args = Namespace(**kwargs)
    args.e10s = context.mozharness_config.get('e10s', args.e10s)

    if not args.tests:
        args.tests = [os.path.join('layout', 'reftests', 'reftest.list')]

    test_root = os.path.join(context.package_root, 'reftest', 'tests')
    normalize = partial(context.normalize_test_path, test_root)
    args.tests = map(normalize, args.tests)

    return run_reftest_desktop(context, args)


def run_reftest_desktop(context, args):
    from runreftest import run_test_harness

    args.app = args.app or context.firefox_bin
    args.extraProfileFiles.append(os.path.join(context.bin_dir, 'plugins'))
    args.utilityPath = context.bin_dir

    return run_test_harness(parser, args)


def setup_argument_parser():
    import reftestcommandline
    global parser
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
        kwargs['suite'] = 'reftest'
        return run_reftest(self.context, **kwargs)
