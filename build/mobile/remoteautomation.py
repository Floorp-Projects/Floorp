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
import subprocess
import sys

from automation import Automation
from mozdevice import DMError, DeviceManager
from mozlog import get_default_logger
from mozscreenshot import dump_screen
import mozcrash

# signatures for logcat messages that we don't care about much
fennecLogcatFilters = [ "The character encoding of the HTML document was not declared",
                        "Use of Mutation Events is deprecated. Use MutationObserver instead.",
                        "Unexpected value from nativeGetEnabledTags: 0" ]

class RemoteAutomation(Automation):
    _devicemanager = None

    # Part of a hack for Robocop: "am COMMAND" is handled specially if COMMAND
    # is in this set. See usages below.
    _specialAmCommands = ('instrument', 'start')

    def __init__(self, deviceManager, appName = '', remoteLog = None,
                 processArgs=None):
        self._devicemanager = deviceManager
        self._appName = appName
        self._remoteProfile = None
        self._remoteLog = remoteLog
        self._processArgs = processArgs or {};

        # Default our product to fennec
        self._product = "fennec"
        self.lastTestSeen = "remoteautomation.py"
        Automation.__init__(self)

    def setDeviceManager(self, deviceManager):
        self._devicemanager = deviceManager

    def setAppName(self, appName):
        self._appName = appName

    def setRemoteProfile(self, remoteProfile):
        self._remoteProfile = remoteProfile

    def setProduct(self, product):
        self._product = product

    def setRemoteLog(self, logfile):
        self._remoteLog = logfile

    # Set up what we need for the remote environment
    def environment(self, env=None, xrePath=None, crashreporter=True, debugger=False, dmdPath=None, lsanPath=None, ubsanPath=None):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        if dmdPath:
            env['MOZ_REPLACE_MALLOC_LIB'] = os.path.join(dmdPath, 'libdmd.so')

        # Except for the mochitest results table hiding option, which isn't
        # passed to runtestsremote.py as an actual option, but through the
        # MOZ_HIDE_RESULTS_TABLE environment variable.
        if 'MOZ_HIDE_RESULTS_TABLE' in os.environ:
            env['MOZ_HIDE_RESULTS_TABLE'] = os.environ['MOZ_HIDE_RESULTS_TABLE']

        if crashreporter and not debugger:
            env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
            env['MOZ_CRASHREPORTER'] = '1'
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
        # env.setdefault('NSPR_LOG_MODULES', 'signaling:5,mtransport:5,datachannel:5,jsep:5,MediaPipelineFactory:5')
        env.setdefault('R_LOG_LEVEL', '6')
        env.setdefault('R_LOG_DESTINATION', 'stderr')
        env.setdefault('R_LOG_VERBOSE', '1')

        return env

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime, debuggerInfo, symbolsPath, outputHandler=None):
        """ Wait for tests to finish.
            If maxTime seconds elapse or no output is detected for timeout
            seconds, kill the process and fail the test.
        """
        proc.utilityPath = utilityPath
        # maxTime is used to override the default timeout, we should honor that
        status = proc.wait(timeout = maxTime, noOutputTimeout = timeout)
        self.lastTestSeen = proc.getLastTestSeen

        topActivity = self._devicemanager.getTopActivity()
        if topActivity == proc.procName:
            print "Browser unexpectedly found running. Killing..."
            proc.kill(True)
        if status == 1:
            if maxTime:
                print "TEST-UNEXPECTED-FAIL | %s | application ran for longer than " \
                      "allowed maximum time of %s seconds" % (self.lastTestSeen, maxTime)
            else:
                print "TEST-UNEXPECTED-FAIL | %s | application ran for longer than " \
                      "allowed maximum time" % (self.lastTestSeen)
        if status == 2:
            print "TEST-UNEXPECTED-FAIL | %s | application timed out after %d seconds with no output" \
                % (self.lastTestSeen, int(timeout))

        return status

    def deleteANRs(self):
        # empty ANR traces.txt file; usually need root permissions
        # we make it empty and writable so we can test the ANR reporter later
        traces = "/data/anr/traces.txt"
        try:
            self._devicemanager.shellCheckOutput(['echo', '', '>', traces], root=True,
                                                 timeout=DeviceManager.short_timeout)
            self._devicemanager.shellCheckOutput(['chmod', '666', traces], root=True,
                                                 timeout=DeviceManager.short_timeout)
        except DMError:
            print "Error deleting %s" % traces
            pass

    def checkForANRs(self):
        traces = "/data/anr/traces.txt"
        if self._devicemanager.fileExists(traces):
            try:
                t = self._devicemanager.pullFile(traces)
                if t:
                    stripped = t.strip()
                    if len(stripped) > 0:
                        print "Contents of %s:" % traces
                        print t
                # Once reported, delete traces
                self.deleteANRs()
            except DMError:
                print "Error pulling %s" % traces
            except IOError:
                print "Error pulling %s" % traces
        else:
            print "%s not found" % traces

    def deleteTombstones(self):
        # delete any existing tombstone files from device
        tombstones = "/data/tombstones/*"
        try:
            self._devicemanager.shellCheckOutput(['rm', '-r', tombstones], root=True,
                                                 timeout=DeviceManager.short_timeout)
        except DMError:
            # This may just indicate that the tombstone directory is missing
            pass

    def checkForTombstones(self):
        # pull any tombstones from device and move to MOZ_UPLOAD_DIR
        remoteDir = "/data/tombstones"
        blobberUploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
        if blobberUploadDir:
            if not os.path.exists(blobberUploadDir):
                os.mkdir(blobberUploadDir)
            if self._devicemanager.dirExists(remoteDir):
                # copy tombstone files from device to local blobber upload directory
                try:
                    self._devicemanager.shellCheckOutput(['chmod', '777', remoteDir], root=True,
                                                 timeout=DeviceManager.short_timeout)
                    self._devicemanager.shellCheckOutput(['chmod', '666', os.path.join(remoteDir, '*')], root=True,
                                                 timeout=DeviceManager.short_timeout)
                    self._devicemanager.getDirectory(remoteDir, blobberUploadDir, False)
                except DMError:
                    # This may just indicate that no tombstone files are present
                    pass
                self.deleteTombstones()
                # add a .txt file extension to each tombstone file name, so
                # that blobber will upload it
                for f in glob.glob(os.path.join(blobberUploadDir, "tombstone_??")):
                    # add a unique integer to the file name, in case there are
                    # multiple tombstones generated with the same name, for
                    # instance, after multiple robocop tests
                    for i in xrange(1, sys.maxint):
                        newname = "%s.%d.txt" % (f, i)
                        if not os.path.exists(newname):
                            os.rename(f, newname)
                            break
            else:
                print "%s does not exist; tombstone check skipped" % remoteDir
        else:
            print "MOZ_UPLOAD_DIR not defined; tombstone check skipped"

    def checkForCrashes(self, directory, symbolsPath):
        self.checkForANRs()
        self.checkForTombstones()

        logcat = self._devicemanager.getLogcat(filterOutRegexps=fennecLogcatFilters)

        javaException = mozcrash.check_for_java_exception(logcat, test_name=self.lastTestSeen)
        if javaException:
            return True

        # If crash reporting is disabled (MOZ_CRASHREPORTER!=1), we can't say
        # anything.
        if not self.CRASHREPORTER:
            return False

        try:
            dumpDir = tempfile.mkdtemp()
            remoteCrashDir = posixpath.join(self._remoteProfile, 'minidumps')
            if not self._devicemanager.dirExists(remoteCrashDir):
                # If crash reporting is enabled (MOZ_CRASHREPORTER=1), the
                # minidumps directory is automatically created when Fennec
                # (first) starts, so its lack of presence is a hint that
                # something went wrong.
                print "Automation Error: No crash directory (%s) found on remote device" % remoteCrashDir
                # Whilst no crash was found, the run should still display as a failure
                return True
            self._devicemanager.getDirectory(remoteCrashDir, dumpDir)

            logger = get_default_logger()
            if logger is not None:
                crashed = mozcrash.log_crashes(logger, dumpDir, symbolsPath, test=self.lastTestSeen)
            else:
                crashed = Automation.checkForCrashes(self, dumpDir, symbolsPath)

        finally:
            try:
                shutil.rmtree(dumpDir)
            except:
                print "WARNING: unable to remove directory: %s" % dumpDir
        return crashed

    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        # If remote profile is specified, use that instead
        if (self._remoteProfile):
            profileDir = self._remoteProfile

        # Hack for robocop, if app & testURL == None and extraArgs contains the rest of the stuff, lets
        # assume extraArgs is all we need
        if app == "am" and extraArgs[0] in RemoteAutomation._specialAmCommands:
            return app, extraArgs

        cmd, args = Automation.buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs)
        # Remove -foreground if it exists, if it doesn't this just returns
        try:
            args.remove('-foreground')
        except:
            pass
#TODO: figure out which platform require NO_EM_RESTART
#        return app, ['--environ:NO_EM_RESTART=1'] + args
        return app, args

    def Process(self, cmd, stdout = None, stderr = None, env = None, cwd = None):
        if stdout == None or stdout == -1 or stdout == subprocess.PIPE:
            stdout = self._remoteLog

        return self.RProcess(self._devicemanager, cmd, stdout, stderr, env, cwd, self._appName,
                             **self._processArgs)

    # be careful here as this inner class doesn't have access to outer class members
    class RProcess(object):
        # device manager process
        dm = None
        def __init__(self, dm, cmd, stdout=None, stderr=None, env=None, cwd=None, app=None,
                     messageLogger=None):
            self.dm = dm
            self.stdoutlen = 0
            self.lastTestSeen = "remoteautomation.py"
            self.proc = dm.launchProcess(cmd, stdout, cwd, env, True)
            self.messageLogger = messageLogger
            self.utilityPath = None

            if (self.proc is None):
                if cmd[0] == 'am':
                    self.proc = stdout
                else:
                    raise Exception("unable to launch process")
            self.procName = cmd[0].split('/')[-1]
            if cmd[0] == 'am' and cmd[1] in RemoteAutomation._specialAmCommands:
                self.procName = app

            # Setting timeout at 1 hour since on a remote device this takes much longer.
            # Temporarily increased to 90 minutes because no more chunks can be created.
            self.timeout = 5400
            # The benefit of the following sleep is unclear; it was formerly 15 seconds
            time.sleep(1)

            # Used to buffer log messages until we meet a line break
            self.logBuffer = ""

        @property
        def pid(self):
            pid = self.dm.processExist(self.procName)
            # HACK: we should probably be more sophisticated about monitoring
            # running processes for the remote case, but for now we'll assume
            # that this method can be called when nothing exists and it is not
            # an error
            if pid is None:
                return 0
            return pid

        def read_stdout(self):
            """
            Fetch the full remote log file using devicemanager, process them and
            return whether there were any new log entries since the last call.
            """
            if not self.dm.fileExists(self.proc):
                return False
            try:
                newLogContent = self.dm.pullFile(self.proc, self.stdoutlen)
            except DMError:
                return False
            if not newLogContent:
                return False

            self.stdoutlen += len(newLogContent)

            if self.messageLogger is None:
                testStartFilenames = re.findall(r"TEST-START \| ([^\s]*)", newLogContent)
                if testStartFilenames:
                    self.lastTestSeen = testStartFilenames[-1]
                print newLogContent
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
        def wait(self, timeout = None, noOutputTimeout = None):
            timer = 0
            noOutputTimer = 0
            interval = 10
            if timeout == None:
                timeout = self.timeout
            status = 0
            top = self.procName
            slowLog = False
            endTime = datetime.datetime.now() + datetime.timedelta(seconds = timeout)
            while (top == self.procName):
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
                    top = self.dm.getTopActivity()
                    if top == "":
                        print "Failed to get top activity, retrying, once..."
                        top = self.dm.getTopActivity()
            # Flush anything added to stdout during the sleep
            self.read_stdout()
            return status

        def kill(self, stagedShutdown = False):
            if self.utilityPath:
                # Take a screenshot to capture the screen state just before
                # the application is killed. There are on-device screenshot
                # options but they rarely work well with Firefox on the
                # Android emulator. dump_screen provides an effective
                # screenshot of the emulator and its host desktop.
                dump_screen(self.utilityPath, get_default_logger())
            if stagedShutdown:
                # Trigger an ANR report with "kill -3" (SIGQUIT)
                self.dm.killProcess(self.procName, 3)
                time.sleep(3)
                # Trigger a breakpad dump with "kill -6" (SIGABRT)
                self.dm.killProcess(self.procName, 6)
                # Wait for process to end
                retries = 0
                while retries < 3:
                    pid = self.dm.processExist(self.procName)
                    if pid and pid > 0:
                        print "%s still alive after SIGABRT: waiting..." % self.procName
                        time.sleep(5)
                    else:
                        return
                    retries += 1
                self.dm.killProcess(self.procName, 9)
                pid = self.dm.processExist(self.procName)
                if pid and pid > 0:
                    self.dm.killProcess(self.procName)
            else:
                self.dm.killProcess(self.procName)
