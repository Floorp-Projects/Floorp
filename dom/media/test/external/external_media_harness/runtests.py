# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

import mozlog

from manifestparser import read_ini
from marionette_harness import (
    BaseMarionetteTestRunner,
    BaseMarionetteArguments,
    BrowserMobProxyArguments,
)
from marionette_harness.runtests import MarionetteHarness, cli as mn_cli

import external_media_tests
from testcase import MediaTestCase
from external_media_tests.media_utils.video_puppeteer import debug_script


class MediaTestArgumentsBase(object):
    name = 'Firefox Media Tests'
    args = [
        [['--urls'], {
            'help': 'ini file of urls to make available to all tests',
            'default': os.path.join(external_media_tests.urls, 'default.ini'),
        }],
    ]

    def verify_usage_handler(self, args):
        if args.urls:
            if not os.path.isfile(args.urls):
                raise ValueError('--urls must provide a path to an ini file')
            else:
                path = os.path.abspath(args.urls)
                args.video_urls = MediaTestArgumentsBase.get_urls(path)
                if not args.video_urls:
                    raise ValueError('list of video URLs cannot be empty')

    def parse_args_handler(self, args):
        if not args.tests:
            args.tests = [external_media_tests.manifest]


    @staticmethod
    def get_urls(manifest):
        with open(manifest, 'r'):
            return [line[0] for line in read_ini(manifest)]


class MediaTestArguments(BaseMarionetteArguments):
    def __init__(self, **kwargs):
        BaseMarionetteArguments.__init__(self, **kwargs)
        self.register_argument_container(MediaTestArgumentsBase())
        self.register_argument_container(BrowserMobProxyArguments())


class MediaTestRunner(BaseMarionetteTestRunner):
    def __init__(self, **kwargs):
        BaseMarionetteTestRunner.__init__(self, **kwargs)
        if not self.server_root:
            self.server_root = external_media_tests.resources
        # pick up prefs from marionette_driver.geckoinstance.DesktopInstance
        self.app = 'fxdesktop'
        self.test_handlers = [MediaTestCase]

        # Used in HTML report (--log-html)
        def gather_media_debug(test, status):
            rv = {}
            marionette = test._marionette_weakref()

            if marionette.session is not None:
                try:
                    with marionette.using_context(marionette.CONTEXT_CHROME):
                        debug_lines = marionette.execute_script(debug_script)
                        if debug_lines:
                            name = 'mozMediaSourceObject.mozDebugReaderData'
                            rv[name] = '\n'.join(debug_lines)
                        else:
                            logger = mozlog.get_default_logger()
                            logger.info('No data available about '
                                        'mozMediaSourceObject')
                except:
                    logger = mozlog.get_default_logger()
                    logger.warning('Failed to gather test failure media debug',
                                   exc_info=True)
            return rv

        self.result_callbacks.append(gather_media_debug)


class FirefoxMediaHarness(MarionetteHarness):
    def parse_args(self, *args, **kwargs):
        return MarionetteHarness.parse_args(self, {'mach': sys.stdout})


def cli():
    mn_cli(MediaTestRunner, MediaTestArguments, FirefoxMediaHarness)

if __name__ == '__main__':
    cli()
