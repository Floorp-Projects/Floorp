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
from devicemanager import NetworkTools, DMError
import mozcrash

# signatures for logcat messages that we don't care about much
fennecLogcatFilters = [ "The character encoding of the HTML document was not declared",
                        "Use of Mutation Events is deprecated. Use MutationObserver instead.",
                        "Unexpected value from nativeGetEnabledTags: 0" ]

class RemoteAutomation(Automation):
    _devicemanager = None

    def __init__(self, deviceManager, appName = '', remoteLog = None):
        self._devicemanager = deviceManager
        self._appName = appName
        self._remoteProfile = None
        self._remoteLog = remoteLog

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
    def environment(self, env=None, xrePath=None, crashreporter=True, debugger=False, dmdPath=None):
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

        return env

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime, debuggerInfo, symbolsPath):
        """ Wait for tests to finish (as evidenced by the process exiting),
            or for maxTime elapse, in which case kill the process regardless.
        """
        # maxTime is used to override the default timeout, we should honor that
        status = proc.wait(timeout = maxTime)
        self.lastTestSeen = proc.getLastTestSeen

        if (status == 1 and self._devicemanager.getTopActivity() == proc.procName):
            # Then we timed out, make sure Fennec is dead
            if maxTime:
                print "TEST-UNEXPECTED-FAIL | %s | application ran for longer than " \
                      "allowed maximum time of %s seconds" % (self.lastTestSeen, maxTime)
            else:
                print "TEST-UNEXPECTED-FAIL | %s | application ran for longer than " \
                      "allowed maximum time" % (self.lastTestSeen)
            proc.kill()

        return status

    def deleteANRs(self):
        # delete ANR traces.txt file; usually need root permissions
        traces = "/data/anr/traces.txt"
        try:
            self._devicemanager.shellCheckOutput(['rm', traces], root=True)
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

    def getLanIp(self):
        nettools = NetworkTools()
        return nettools.getLanIp()

    def Process(self, cmd, stdout = None, stderr = None, env = None, cwd = None):
        if stdout == None or stdout == -1 or stdout == subprocess.PIPE:
          stdout = self._remoteLog

        return self.RProcess(self._devicemanager, cmd, stdout, stderr, env, cwd)

    # be careful here as this inner class doesn't have access to outer class members
    class RProcess(object):
        # device manager process
        dm = None
        def __init__(self, dm, cmd, stdout = None, stderr = None, env = None, cwd = None):
            self.dm = dm
            self.stdoutlen = 0
            self.lastTestSeen = "remoteautomation.py"
            self.proc = dm.launchProcess(cmd, stdout, cwd, env, True)
            if (self.proc is None):
              if cmd[0] == 'am':
                self.proc = stdout
              else:
                raise Exception("unable to launch process")
            exepath = cmd[0]
            name = exepath.split('/')[-1]
            self.procName = name
            # Hack for Robocop: Derive the actual process name from the command line.
            # We expect something like:
            #  ['am', 'instrument', '-w', '-e', 'class', 'org.mozilla.fennec.tests.testBookmark', 'org.mozilla.roboexample.test/android.test.InstrumentationTestRunner']
            # and want to derive 'org.mozilla.fennec'.
            if cmd[0] == 'am' and cmd[1] == "instrument":
              try:
                i = cmd.index("class")
              except ValueError:
                # no "class" argument -- maybe this isn't robocop?
                i = -1
              if (i > 0):
                classname = cmd[i+1]
                parts = classname.split('.')
                try:
                  i = parts.index("tests")
                except ValueError:
                  # no "tests" component -- maybe this isn't robocop?
                  i = -1
                if (i > 0):
                  self.procName = '.'.join(parts[0:i])
                  print "Robocop derived process name: "+self.procName

            # Setting timeout at 1 hour since on a remote device this takes much longer
            self.timeout = 3600
            # The benefit of the following sleep is unclear; it was formerly 15 seconds
            time.sleep(1)

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

        @property
        def stdout(self):
            """ Fetch the full remote log file using devicemanager and return just
                the new log entries since the last call (as a multi-line string).
            """
            if self.dm.fileExists(self.proc):
                try:
                    newLogContent = self.dm.pullFile(self.proc, self.stdoutlen)
                except DMError:
                    # we currently don't retry properly in the pullFile
                    # function in dmSUT, so an error here is not necessarily
                    # the end of the world
                    return ''
                self.stdoutlen += len(newLogContent)
                # Match the test filepath from the last TEST-START line found in the new
                # log content. These lines are in the form:
                # 1234 INFO TEST-START | /filepath/we/wish/to/capture.html\n
                testStartFilenames = re.findall(r"TEST-START \| ([^\s]*)", newLogContent)
                if testStartFilenames:
                    self.lastTestSeen = testStartFilenames[-1]
                return newLogContent.strip('\n').strip()
            else:
                return ''

        @property
        def getLastTestSeen(self):
            return self.lastTestSeen

        def wait(self, timeout = None):
            timer = 0
            interval = 20 

            if timeout == None:
                timeout = self.timeout

            while (self.dm.getTopActivity() == self.procName):
                # retrieve log updates every 60 seconds
                if timer % 60 == 0: 
                    t = self.stdout
                    if t != '':
                        print t

                time.sleep(interval)
                timer += interval
                if (timer > timeout):
                    break

            # Flush anything added to stdout during the sleep
            print self.stdout

            if (timer >= timeout):
                return 1
            return 0

        def kill(self):
            self.dm.killProcess(self.procName)
