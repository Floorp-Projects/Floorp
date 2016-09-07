# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
from functools import partial

from mach.decorators import (
    CommandProvider,
    Command,
)


def run_reftest(context, **kwargs):
    kwargs['app'] = kwargs['app'] or context.firefox_bin
    kwargs['e10s'] = context.mozharness_config.get('e10s', kwargs['e10s'])
    kwargs['certPath'] = context.certs_dir
    kwargs['utilityPath'] = context.bin_dir
    kwargs['extraProfileFiles'].append(os.path.join(context.bin_dir, 'plugins'))

    if not kwargs['tests']:
        kwargs['tests'] = [os.path.join('layout', 'reftests', 'reftest.list')]

    test_root = os.path.join(context.package_root, 'reftest', 'tests')
    normalize = partial(context.normalize_test_path, test_root)
    kwargs['tests'] = map(normalize, kwargs['tests'])

    from runreftest import run as run_test_harness
    return run_test_harness(**kwargs)


def setup_argument_parser():
    from reftestcommandline import DesktopArgumentsParser
    return DesktopArgumentsParser()


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
