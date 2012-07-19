# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
import sys
import os
import socket
import automationutils
import tempfile
import shutil

from automation import Automation
from devicemanager import DeviceManager, NetworkTools

class RemoteAutomation(Automation):
    _devicemanager = None
    
    def __init__(self, deviceManager, appName = '', remoteLog = None):
        self._devicemanager = deviceManager
        self._appName = appName
        self._remoteProfile = None
        self._remoteLog = remoteLog

        # Default our product to fennec
        self._product = "fennec"
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
    def environment(self, env = None, xrePath = None, crashreporter = True):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        # Except for the mochitest results table hiding option, which isn't
        # passed to runtestsremote.py as an actual option, but through the
        # MOZ_CRASHREPORTER_DISABLE environment variable.
        if 'MOZ_HIDE_RESULTS_TABLE' in os.environ:
            env['MOZ_HIDE_RESULTS_TABLE'] = os.environ['MOZ_HIDE_RESULTS_TABLE']

        if crashreporter:
            env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
            env['MOZ_CRASHREPORTER'] = '1'
        else:
            env['MOZ_CRASHREPORTER_DISABLE'] = '1'

        return env

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime, debuggerInfo, symbolsDir, logger):
        # maxTime is used to override the default timeout, we should honor that
        status = proc.wait(timeout = maxTime)

        print proc.stdout

        if (status == 1 and self._devicemanager.processExist(proc.procName)):
            # Then we timed out, make sure Fennec is dead
            proc.kill()

        return status

    def checkForCrashes(self, directory, symbolsPath):
        dumpDir = tempfile.mkdtemp()
        self._devicemanager.getDirectory(self._remoteProfile + '/minidumps/', dumpDir)
        automationutils.checkForCrashes(dumpDir, symbolsPath, self.lastTestSeen)
        try:
          shutil.rmtree(dumpDir)
        except:
          print "WARNING: unable to remove directory: %s" % (dumpDir)

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
            time.sleep(15)

        @property
        def pid(self):
            hexpid = self.dm.processExist(self.procName)
            if (hexpid == None):
                hexpid = "0x0"
            return int(hexpid, 0)
    
        @property
        def stdout(self):
            t = self.dm.getFile(self.proc)
            if t == None: return ''
            tlen = len(t)
            retVal = t[self.stdoutlen:]
            self.stdoutlen = tlen
            return retVal.strip('\n').strip()
 
        def wait(self, timeout = None):
            timer = 0
            interval = 5

            if timeout == None:
                timeout = self.timeout

            while (self.dm.processExist(self.procName)):
                t = self.stdout
                if t != '': print t
                time.sleep(interval)
                timer += interval
                if (timer > timeout):
                    break

            if (timer >= timeout):
                return 1
            return 0
 
        def kill(self):
            self.dm.killProcess(self.procName)
