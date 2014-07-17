# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
import re
import os
import tempfile
import shutil
import subprocess

from automation import Automation
from devicemanager import DMError
import mozcrash

# signatures for logcat messages that we don't care about much
fennecLogcatFilters = [ "The character encoding of the HTML document was not declared",
                        "Use of Mutation Events is deprecated. Use MutationObserver instead.",
                        "Unexpected value from nativeGetEnabledTags: 0" ]

class RemoteAutomation(Automation):
    _devicemanager = None

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
    def environment(self, env=None, xrePath=None, crashreporter=True, debugger=False, dmdPath=None, lsanPath=None):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        if dmdPath:
            env['DMD'] = '1'
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

        # Crash on non-local network connections.
        env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '1'

        return env

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime, debuggerInfo, symbolsPath):
        """ Wait for tests to finish.
            If maxTime seconds elapse or no output is detected for timeout
            seconds, kill the process and fail the test.
        """
        # maxTime is used to override the default timeout, we should honor that
        status = proc.wait(timeout = maxTime, noOutputTimeout = timeout)
        self.lastTestSeen = proc.getLastTestSeen

        topActivity = self._devicemanager.getTopActivity()
        if topActivity == proc.procName:
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
            self._devicemanager.shellCheckOutput(['echo', '', '>', traces], root=True)
            self._devicemanager.shellCheckOutput(['chmod', '666', traces], root=True)
        except DMError:
            print "Error deleting %s" % traces
            pass

    def checkForANRs(self):
        traces = "/data/anr/traces.txt"
        if self._devicemanager.fileExists(traces):
            try:
                t = self._devicemanager.pullFile(traces)
                print "Contents of %s:" % traces
                print t
                # Once reported, delete traces
                self.deleteANRs()
            except DMError:
                print "Error pulling %s" % traces
                pass
        else:
            print "%s not found" % traces

    def checkForCrashes(self, directory, symbolsPath):
        self.checkForANRs()

        logcat = self._devicemanager.getLogcat(filterOutRegexps=fennecLogcatFilters)
        javaException = mozcrash.check_for_java_exception(logcat)
        if javaException:
            return True

        # If crash reporting is disabled (MOZ_CRASHREPORTER!=1), we can't say
        # anything.
        if not self.CRASHREPORTER:
            return False

        try:
            dumpDir = tempfile.mkdtemp()
            remoteCrashDir = self._remoteProfile + '/minidumps/'
            if not self._devicemanager.dirExists(remoteCrashDir):
                # If crash reporting is enabled (MOZ_CRASHREPORTER=1), the
                # minidumps directory is automatically created when Fennec
                # (first) starts, so its lack of presence is a hint that
                # something went wrong.
                print "Automation Error: No crash directory (%s) found on remote device" % remoteCrashDir
                # Whilst no crash was found, the run should still display as a failure
                return True
            self._devicemanager.getDirectory(remoteCrashDir, dumpDir)
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
        if app == "am" and extraArgs[0] == "instrument":
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

            if (self.proc is None):
                if cmd[0] == 'am':
                    self.proc = stdout
                else:
                    raise Exception("unable to launch process")
            self.procName = cmd[0].split('/')[-1]
            if cmd[0] == 'am' and cmd[1] == "instrument":
                self.procName = app
                print "Robocop process name: "+self.procName

            # Setting timeout at 1 hour since on a remote device this takes much longer
            self.timeout = 3600
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
            """ Fetch the full remote log file using devicemanager and return just
                the new log entries since the last call (as a list of messages or lines).
            """
            if not self.dm.fileExists(self.proc):
                return []
            try:
                newLogContent = self.dm.pullFile(self.proc, self.stdoutlen)
            except DMError:
                # we currently don't retry properly in the pullFile
                # function in dmSUT, so an error here is not necessarily
                # the end of the world
                return []
            if not newLogContent:
                return []

            self.stdoutlen += len(newLogContent)

            if self.messageLogger is None:
                testStartFilenames = re.findall(r"TEST-START \| ([^\s]*)", newLogContent)
                if testStartFilenames:
                    self.lastTestSeen = testStartFilenames[-1]
                print newLogContent
                return [newLogContent]

            self.logBuffer += newLogContent
            lines = self.logBuffer.split('\n')
            if not lines:
                return

            # We only keep the last (unfinished) line in the buffer
            self.logBuffer = lines[-1]
            del lines[-1]
            messages = []
            for line in lines:
                # This passes the line to the logger (to be logged or buffered)
                # and returns a list of structured messages (dict) or None, depending on the log
                parsed_messages = self.messageLogger.write(line)
                for message in parsed_messages:
                    if message['action'] == 'test_start':
                        self.lastTestSeen = message['test']
                messages += parsed_messages
            return messages

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
            interval = 20

            if timeout == None:
                timeout = self.timeout

            status = 0
            while (self.dm.getTopActivity() == self.procName):
                # retrieve log updates every 60 seconds
                if timer % 60 == 0:
                    messages = self.read_stdout()
                    if messages:
                        noOutputTimer = 0

                time.sleep(interval)
                timer += interval
                noOutputTimer += interval
                if (timer > timeout):
                    status = 1
                    break
                if (noOutputTimeout and noOutputTimer > noOutputTimeout):
                    status = 2
                    break

            # Flush anything added to stdout during the sleep
            self.read_stdout()

            return status

        def kill(self, stagedShutdown = False):
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
