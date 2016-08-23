# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import sys

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mach.decorators import (
    CommandProvider,
    Command,
)

def setup_argument_parser():
    from external_media_harness.runtests import MediaTestArguments
    from mozlog.structured import commandline
    parser = MediaTestArguments()
    commandline.add_logging_group(parser)
    return parser


def run_external_media_test(tests, testtype=None, topsrcdir=None, **kwargs):
    from external_media_harness.runtests import (
        FirefoxMediaHarness,
        MediaTestArguments,
        MediaTestRunner,
        mn_cli,
    )

    from mozlog.structured import commandline

    from argparse import Namespace

    parser = setup_argument_parser()

    if not tests:
        tests = [os.path.join(topsrcdir,
                 'dom/media/test/external/external_media_tests/manifest.ini')]

    args = Namespace(tests=tests)

    for k, v in kwargs.iteritems():
        setattr(args, k, v)

    parser.verify_usage(args)

    args.logger = commandline.setup_logging("Firefox External Media Tests",
                                            args,
                                            {"mach": sys.stdout})
    failed = mn_cli(MediaTestRunner, MediaTestArguments, FirefoxMediaHarness,
                    args=vars(args))

    if failed > 0:
        return 1
    else:
        return 0


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('external-media-tests', category='testing',
             description='Run Firefox external media tests.',
             conditions=[conditions.is_firefox],
             parser=setup_argument_parser,
             )
    def run_external_media_test(self, tests, **kwargs):
        kwargs['binary'] = kwargs['binary'] or self.get_binary_path('app')
        return run_external_media_test(tests, topsrcdir=self.topsrcdir, **kwargs)
