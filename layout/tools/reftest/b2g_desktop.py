# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function, unicode_literals

import json
import os
import signal
import sys
import threading

here = os.path.abspath(os.path.dirname(__file__))

from runreftest import RefTest, ReftestOptions

from marionette import Marionette
from mozprocess import ProcessHandler
from mozrunner import FirefoxRunner
import mozinfo
import mozlog

log = mozlog.getLogger('REFTEST')

class B2GDesktopReftest(RefTest):
    def __init__(self, marionette):
        RefTest.__init__(self)
        self.last_test = os.path.basename(__file__)
        self.marionette = marionette
        self.profile = None
        self.runner = None
        self.test_script = os.path.join(here, 'b2g_start_script.js')
        self.timeout = None

    def run_marionette_script(self):
        assert(self.marionette.wait_for_port())
        self.marionette.start_session()
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        if os.path.isfile(self.test_script):
            f = open(self.test_script, 'r')
            self.test_script = f.read()
            f.close()
        self.marionette.execute_script(self.test_script)

    def run_tests(self, test_path, options):
        reftestlist = self.getManifestPath(test_path)
        if not reftestlist.startswith('file://'):
            reftestlist = 'file://%s' % reftestlist

        self.profile = self.create_profile(options, reftestlist,
                                           profile_to_clone=options.profile)
        env = self.buildBrowserEnv(options, self.profile.profile)
        kp_kwargs = { 'processOutputLine': [self._on_output],
                      'onTimeout': [self._on_timeout],
                      'kill_on_timeout': False }

        if not options.debugger:
            if not options.timeout:
                if mozinfo.info['debug']:
                    options.timeout = 420
                else:
                    options.timeout = 300
            self.timeout = options.timeout + 30.0

        log.info("%s | Running tests: start.", os.path.basename(__file__))
        cmd, args = self.build_command_line(options.app,
                            ignore_window_size=options.ignoreWindowSize,
                            browser_arg=options.browser_arg)
        self.runner = FirefoxRunner(profile=self.profile,
                                    binary=cmd,
                                    cmdargs=args,
                                    env=env,
                                    process_class=ProcessHandler,
                                    symbols_path=options.symbolsPath,
                                    kp_kwargs=kp_kwargs)

        status = 0
        try:
            self.runner.start(outputTimeout=self.timeout)
            log.info("%s | Application pid: %d",
                     os.path.basename(__file__),
                     self.runner.process_handler.pid)

            # kick starts the reftest harness
            self.run_marionette_script()
            status = self.runner.wait()
        finally:
            self.runner.check_for_crashes(test_name=self.last_test)
            self.runner.cleanup()

        if status > 0:
            log.testFail("%s | application terminated with exit code %s",
                         self.last_test, status)
        elif status < 0:
            log.info("%s | application killed with signal %s",
                         self.last_test, -status)

        log.info("%s | Running tests: end.", os.path.basename(__file__))
        return status

    def create_profile(self, options, reftestlist, profile_to_clone=None):
        profile = RefTest.createReftestProfile(self, options, reftestlist,
                                               profile_to_clone=profile_to_clone)

        prefs = {}
        # Turn off the locale picker screen
        prefs["browser.firstrun.show.localepicker"] = False
        prefs["b2g.system_startup_url"] = "app://test-container.gaiamobile.org/index.html"
        prefs["b2g.system_manifest_url"] = "app://test-container.gaiamobile.org/manifest.webapp"
        prefs["browser.tabs.remote"] = False
        prefs["dom.ipc.tabs.disabled"] = False
        prefs["dom.mozBrowserFramesEnabled"] = True
        prefs["font.size.inflation.emPerLine"] = 0
        prefs["font.size.inflation.minTwips"] = 0
        prefs["network.dns.localDomains"] = "app://test-container.gaiamobile.org"
        prefs["reftest.browser.iframe.enabled"] = False
        prefs["reftest.remote"] = False
        prefs["reftest.uri"] = "%s" % reftestlist
        # Set a future policy version to avoid the telemetry prompt.
        prefs["toolkit.telemetry.prompted"] = 999
        prefs["toolkit.telemetry.notifiedOptOut"] = 999

        # Set the extra prefs.
        profile.set_preferences(prefs)
        return profile

    def build_command_line(self, app, ignore_window_size=False,
                           browser_arg=None):
        cmd = os.path.abspath(app)
        args = ['-marionette']

        if browser_arg:
            args += [browser_arg]

        if not ignore_window_size:
            args.extend(['--screen', '800x1000'])
        return cmd, args

    def _on_output(self, line):
        print(line)
        # TODO use structured logging
        if "TEST-START" in line and "|" in line:
            self.last_test = line.split("|")[1].strip()

    def _on_timeout(self):
        msg = "%s | application timed out after %s seconds with no output"
        log.testFail(msg % (self.last_test, self.timeout))

        # kill process to get a stack
        self.runner.stop(sig=signal.SIGABRT)


def run_desktop_reftests(parser, options, args):
    kwargs = {}
    if options.marionette:
        host, port = options.marionette.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)
    marionette = Marionette.getMarionetteOrExit(**kwargs)

    reftest = B2GDesktopReftest(marionette)

    options = ReftestOptions.verifyCommonOptions(parser, options, reftest)
    if options == None:
        sys.exit(1)

    # add a -bin suffix if b2g-bin exists, but just b2g was specified
    if options.app[-4:] != '-bin':
        if os.path.isfile("%s-bin" % options.app):
            options.app = "%s-bin" % options.app

    if options.desktop and not options.profile:
        raise Exception("must specify --profile when specifying --desktop")

    sys.exit(reftest.run_tests(args[0], options))
