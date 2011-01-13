# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Joel Maher.
#
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
# Joel Maher <joel.maher@gmail.com> (Original Developer)
# Clint Talbert <cmtalbert@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import time
import sys
import os
import socket

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

        if crashreporter:
            env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
            env['MOZ_CRASHREPORTER'] = '1'
        else:
            env['MOZ_CRASHREPORTER_DISABLE'] = '1'

        return env

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime, debuggerInfo, symbolsDir):
        # maxTime is used to override the default timeout, we should honor that
        status = proc.wait(timeout = maxTime)

        print proc.stdout

        if (status == 1 and self._devicemanager.processExist(proc.procName)):
            # Then we timed out, make sure Fennec is dead
            proc.kill()

        return status

    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        # If remote profile is specified, use that instead
        if (self._remoteProfile):
            profileDir = self._remoteProfile

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

    def Process(self, cmd, stdout = None, stderr = None, env = None, cwd = '.'):
        if stdout == None or stdout == -1:
          stdout = self._remoteLog

        return self.RProcess(self._devicemanager, cmd, stdout, stderr, env, cwd)

    # be careful here as this inner class doesn't have access to outer class members    
    class RProcess(object):
        # device manager process
        dm = None
        def __init__(self, dm, cmd, stdout = None, stderr = None, env = None, cwd = '.'):
            self.dm = dm
            self.proc = dm.launchProcess(cmd, stdout, cwd, env)
            exepath = cmd[0]
            name = exepath.split('/')[-1]
            self.procName = name

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
            return self.dm.getFile(self.proc)
 
        def wait(self, timeout = None):
            timer = 0
            interval = 5

            if timeout == None:
                timeout = self.timeout

            while (self.dm.processExist(self.procName)):
                time.sleep(interval)
                timer += interval
                if (timer > timeout):
                    break

            if (timer >= timeout):
                return 1
            return 0
 
        def kill(self):
            self.dm.killProcess(self.procName)
