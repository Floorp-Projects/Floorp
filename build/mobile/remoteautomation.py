# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import glob
import time
import re
import os
import posixpath
import tempfile
import shutil
import sys

from automation import Automation
from mozlog import get_default_logger
from mozscreenshot import dump_screen
import mozcrash

# signatures for logcat messages that we don't care about much
fennecLogcatFilters = ["The character encoding of the HTML document was not declared",
                       "Use of Mutation Events is deprecated. Use MutationObserver instead.",
                       "Unexpected value from nativeGetEnabledTags: 0"]


class RemoteAutomation(Automation):

    def __init__(self, device, appName='', remoteProfile=None, remoteLog=None,
                 processArgs=None):
        self._device = device
        self._appName = appName
        self._remoteProfile = remoteProfile
        self._remoteLog = remoteLog
        self._processArgs = processArgs or {}

        self.lastTestSeen = "remoteautomation.py"
        Automation.__init__(self)

    # Set up what we need for the remote environment
    def environment(self, env=None, xrePath=None, crashreporter=True, debugger=False,
                    lsanPath=None, ubsanPath=None):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        if crashreporter and not debugger:
            env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
            env['MOZ_CRASHREPORTER'] = '1'
            env['MOZ_CRASHREPORTER_SHUTDOWN'] = '1'
        else:
            env['MOZ_CRASHREPORTER_DISABLE'] = '1'

        # Crash on non-local network connections by default.
        # MOZ_DISABLE_NONLOCAL_CONNECTIONS can be set to "0" to temporarily
        # enable non-local connections for the purposes of local testing.
        # Don't override the user's choice here.  See bug 1049688.
        env.setdefault('MOZ_DISABLE_NONLOCAL_CONNECTIONS', '1')

        # Send an env var noting that we are in automation. Passing any
        # value except the empty string will declare the value to exist.
        #
        # This may be used to disabled network connections during testing, e.g.
        # Switchboard & telemetry uploads.
        env.setdefault('MOZ_IN_AUTOMATION', '1')

        # Set WebRTC logging in case it is not set yet.
        # On Android, environment variables cannot contain ',' so the
        # standard WebRTC setting for NSPR_LOG_MODULES is not available.
        # env.setdefault('NSPR_LOG_MODULES', 'signaling:5,mtransport:5,datachannel:5,jsep:5,MediaPipelineFactory:5')  # NOQA: E501
        env.setdefault('R_LOG_LEVEL', '6')
        env.setdefault('R_LOG_DESTINATION', 'stderr')
        env.setdefault('R_LOG_VERBOSE', '1')

        return env

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime, debuggerInfo,
                      symbolsPath, outputHandler=None):
        """ Wait for tests to finish.
            If maxTime seconds elapse or no output is detected for timeout
            seconds, kill the process and fail the test.
        """
        proc.utilityPath = utilityPath
        # maxTime is used to override the default timeout, we should honor that
        status = proc.wait(timeout=maxTime, noOutputTimeout=timeout)
        self.lastTestSeen = proc.getLastTestSeen

        topActivity = self._device.get_top_activity(timeout=60)
        if topActivity == proc.procName:
            print("Browser unexpectedly found running. Killing...")
            proc.kill(True)
        if status == 1:
            if maxTime:
                print("TEST-UNEXPECTED-FAIL | %s | application ran for longer than "
                      "allowed maximum time of %s seconds" % (
                          self.lastTestSeen, maxTime))
            else:
                print("TEST-UNEXPECTED-FAIL | %s | application ran for longer than "
                      "allowed maximum time" % (self.lastTestSeen))
        if status == 2:
            print("TEST-UNEXPECTED-FAIL | %s | application timed out after %d "
                  "seconds with no output"
                  % (self.lastTestSeen, int(timeout)))

        return status

    def deleteANRs(self):
        # empty ANR traces.txt file; usually need root permissions
        # we make it empty and writable so we can test the ANR reporter later
        traces = "/data/anr/traces.txt"
        try:
            self._device.shell_output('echo > %s' % traces, root=True)
            self._device.shell_output('chmod 666 %s' % traces, root=True)
        except Exception as e:
            print("Error deleting %s: %s" % (traces, str(e)))

    def checkForANRs(self):
        traces = "/data/anr/traces.txt"
        if self._device.is_file(traces):
            try:
                t = self._device.get_file(traces)
                if t:
                    stripped = t.strip()
                    if len(stripped) > 0:
                        print("Contents of %s:" % traces)
                        print(t)
                # Once reported, delete traces
                self.deleteANRs()
            except Exception as e:
                print("Error pulling %s: %s" % (traces, str(e)))
        else:
            print("%s not found" % traces)

    def deleteTombstones(self):
        # delete any tombstone files from device
        self._device.rm("/data/tombstones", force=True,
                        recursive=True, root=True)

    def checkForTombstones(self):
        # pull any tombstones from device and move to MOZ_UPLOAD_DIR
        remoteDir = "/data/tombstones"
        uploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
        if uploadDir:
            if not os.path.exists(uploadDir):
                os.mkdir(uploadDir)
            if self._device.is_dir(remoteDir):
                # copy tombstone files from device to local upload directory
                self._device.chmod(remoteDir, recursive=True, root=True)
                self._device.pull(remoteDir, uploadDir)
                self.deleteTombstones()
                for f in glob.glob(os.path.join(uploadDir, "tombstone_??")):
                    # add a unique integer to the file name, in case there are
                    # multiple tombstones generated with the same name, for
                    # instance, after multiple robocop tests
                    for i in xrange(1, sys.maxint):
                        newname = "%s.%d.txt" % (f, i)
                        if not os.path.exists(newname):
                            os.rename(f, newname)
                            break
            else:
                print("%s does not exist; tombstone check skipped" % remoteDir)
        else:
            print("MOZ_UPLOAD_DIR not defined; tombstone check skipped")

    def checkForCrashes(self, directory, symbolsPath):
        self.checkForANRs()
        self.checkForTombstones()

        logcat = self._device.get_logcat(
            filter_out_regexps=fennecLogcatFilters)

        javaException = mozcrash.check_for_java_exception(
            logcat, test_name=self.lastTestSeen)
        if javaException:
            return True

        # If crash reporting is disabled (MOZ_CRASHREPORTER!=1), we can't say
        # anything.
        if not self.CRASHREPORTER:
            return False

        try:
            dumpDir = tempfile.mkdtemp()
            remoteCrashDir = posixpath.join(self._remoteProfile, 'minidumps')
            if not self._device.is_dir(remoteCrashDir):
                # If crash reporting is enabled (MOZ_CRASHREPORTER=1), the
                # minidumps directory is automatically created when Fennec
                # (first) starts, so its lack of presence is a hint that
                # something went wrong.
                print("Automation Error: No crash directory (%s) found on remote device" %
                      remoteCrashDir)
                return True
            self._device.pull(remoteCrashDir, dumpDir)

            logger = get_default_logger()
            crashed = mozcrash.log_crashes(
                logger, dumpDir, symbolsPath, test=self.lastTestSeen)

        finally:
            try:
                shutil.rmtree(dumpDir)
            except Exception as e:
                print("WARNING: unable to remove directory %s: %s" % (
                    dumpDir, str(e)))
        return crashed

    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        # If remote profile is specified, use that instead
        if self._remoteProfile:
            profileDir = self._remoteProfile

        # Hack for robocop, if app is "am" and extraArgs contains the rest of the stuff, lets
        # assume extraArgs is all we need
        if app == "am" and extraArgs[0] in ('instrument', 'start'):
            return app, extraArgs

        cmd, args = Automation.buildCommandLine(
            self, app, debuggerInfo, profileDir, testURL, extraArgs)
        try:
            args.remove('-foreground')
        except Exception:
            pass
        return app, args

    def Process(self, cmd, stdout=None, stderr=None, env=None, cwd=None):
        return self.RProcess(self._device, cmd, self._remoteLog, env, cwd, self._appName,
                             **self._processArgs)

    class RProcess(object):
        def __init__(self, device, cmd, stdout=None, env=None, cwd=None, app=None,
                     messageLogger=None, counts=None):
            self.device = device
            self.lastTestSeen = "remoteautomation.py"
            self.messageLogger = messageLogger
            self.proc = stdout
            self.procName = cmd[0].split(posixpath.sep)[-1]
            self.stdoutlen = 0
            self.utilityPath = None

            self.counts = counts
            if self.counts is not None:
                self.counts['pass'] = 0
                self.counts['fail'] = 0
                self.counts['todo'] = 0

            if cmd[0] == 'am':
                cmd = ' '.join(cmd)
                self.procName = app
                if not self.device.shell_bool(cmd):
                    print("remote_automation.py failed to launch %s" % cmd)
            else:
                args = cmd
                if args[0] == app:
                    args = args[1:]
                url = args[-1:][0]
                if url.startswith('/'):
                    # this is probably a reftest profile directory, not a url
                    url = None
                else:
                    args = args[:-1]
                if 'geckoview' in app:
                    activity = "TestRunnerActivity"
                    self.device.launch_activity(app, activity, e10s=True, moz_env=env,
                                                extra_args=args, url=url)
                else:
                    self.device.launch_fennec(
                        app, moz_env=env, extra_args=args, url=url)

            # Setting timeout at 1 hour since on a remote device this takes much longer.
            # Temporarily increased to 110 minutes because no more chunks can be created.
            self.timeout = 6600

            # Used to buffer log messages until we meet a line break
            self.logBuffer = ""

        @property
        def pid(self):
            procs = self.device.get_process_list()
            # limit the comparison to the first 75 characters due to a
            # limitation in processname length in android.
            pids = [proc[0] for proc in procs if proc[1] == self.procName[:75]]

            if pids is None or len(pids) < 1:
                return 0
            return pids[0]

        def read_stdout(self):
            """
            Fetch the full remote log file, log any new content and return True if new
            content processed.
            """
            if not self.device.is_file(self.proc):
                return False
            try:
                newLogContent = self.device.get_file(
                    self.proc, offset=self.stdoutlen)
            except Exception:
                return False
            if not newLogContent:
                return False

            self.stdoutlen += len(newLogContent)

            if self.messageLogger is None:
                testStartFilenames = re.findall(
                    r"TEST-START \| ([^\s]*)", newLogContent)
                if testStartFilenames:
                    self.lastTestSeen = testStartFilenames[-1]
                print(newLogContent)
                return True

            self.logBuffer += newLogContent
            lines = self.logBuffer.split('\n')
            lines = [l for l in lines if l]

            if lines:
                if self.logBuffer.endswith('\n'):
                    # all lines are complete; no need to buffer
                    self.logBuffer = ""
                else:
                    # keep the last (unfinished) line in the buffer
                    self.logBuffer = lines[-1]
                    del lines[-1]

            if not lines:
                return False

            for line in lines:
                # This passes the line to the logger (to be logged or buffered)
                parsed_messages = self.messageLogger.write(line)
                for message in parsed_messages:
                    if isinstance(message, dict) and message.get('action') == 'test_start':
                        self.lastTestSeen = message['test']
                    if isinstance(message, dict) and message.get('action') == 'log':
                        line = message['message'].strip()
                        if self.counts:
                            m = re.match(".*:\s*(\d*)", line)
                            if m:
                                try:
                                    val = int(m.group(1))
                                    if "Passed:" in line:
                                        self.counts['pass'] += val
                                    elif "Failed:" in line:
                                        self.counts['fail'] += val
                                    elif "Todo:" in line:
                                        self.counts['todo'] += val
                                except Exception:
                                    pass

            return True

        @property
        def getLastTestSeen(self):
            return self.lastTestSeen

        # Wait for the remote process to end (or for its activity to go to background).
        # While waiting, periodically retrieve the process output and print it.
        # If the process is still running after *timeout* seconds, return 1;
        # If the process is still running but no output is received in *noOutputTimeout*
        # seconds, return 2;
        # Else, once the process exits/goes to background, return 0.
        def wait(self, timeout=None, noOutputTimeout=None):
            timer = 0
            noOutputTimer = 0
            interval = 10
            if timeout is None:
                timeout = self.timeout
            status = 0
            top = self.procName
            slowLog = False
            endTime = datetime.datetime.now() + datetime.timedelta(seconds=timeout)
            while top == self.procName:
                # Get log updates on each interval, but if it is taking
                # too long, only do it every 60 seconds
                hasOutput = False
                if (not slowLog) or (timer % 60 == 0):
                    startRead = datetime.datetime.now()
                    hasOutput = self.read_stdout()
                    if (datetime.datetime.now() - startRead) > datetime.timedelta(seconds=5):
                        slowLog = True
                    if hasOutput:
                        noOutputTimer = 0
                    if self.counts and 'pass' in self.counts and self.counts['pass'] > 0:
                        interval = 0.5
                time.sleep(interval)
                timer += interval
                noOutputTimer += interval
                if datetime.datetime.now() > endTime:
                    status = 1
                    break
                if (noOutputTimeout and noOutputTimer > noOutputTimeout):
                    status = 2
                    break
                if not hasOutput:
                    top = self.device.get_top_activity(timeout=60)
                    if top is None:
                        print("Failed to get top activity, retrying, once...")
                        top = self.device.get_top_activity(timeout=60)
            # Flush anything added to stdout during the sleep
            self.read_stdout()
            return status

        def kill(self, stagedShutdown=False):
            if self.utilityPath:
                # Take a screenshot to capture the screen state just before
                # the application is killed. There are on-device screenshot
                # options but they rarely work well with Firefox on the
                # Android emulator. dump_screen provides an effective
                # screenshot of the emulator and its host desktop.
                dump_screen(self.utilityPath, get_default_logger())
            if stagedShutdown:
                # Trigger an ANR report with "kill -3" (SIGQUIT)
                try:
                    self.device.pkill(self.procName, sig=3, attempts=1)
                except:  # NOQA: E722
                    pass
                time.sleep(3)
                # Trigger a breakpad dump with "kill -6" (SIGABRT)
                try:
                    self.device.pkill(self.procName, sig=6, attempts=1)
                except:  # NOQA: E722
                    pass
                # Wait for process to end
                retries = 0
                while retries < 3:
                    if self.device.process_exist(self.procName):
                        print("%s still alive after SIGABRT: waiting..." % self.procName)
                        time.sleep(5)
                    else:
                        return
                    retries += 1
                try:
                    self.device.pkill(self.procName, sig=9, attempts=1)
                except:  # NOQA: E722
                    print("%s still alive after SIGKILL!" % self.procName)
                if self.device.process_exist(self.procName):
                    self.device.stop_application(self.procName)
            else:
                self.device.stop_application(self.procName)
