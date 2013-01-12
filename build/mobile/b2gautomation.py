# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import automationutils
import threading
import os
import Queue
import re
import shutil
import tempfile
import time

from automation import Automation
from devicemanager import NetworkTools
from mozprocess import ProcessHandlerMixin


class StdOutProc(ProcessHandlerMixin):
    """Process handler for b2g which puts all output in a Queue.
    """

    def __init__(self, cmd, queue, **kwargs):
        self.queue = queue
        kwargs.setdefault('processOutputLine', []).append(self.handle_output)
        ProcessHandlerMixin.__init__(self, cmd, **kwargs)

    def handle_output(self, line):
        self.queue.put_nowait(line)


class B2GRemoteAutomation(Automation):
    _devicemanager = None

    def __init__(self, deviceManager, appName='', remoteLog=None,
                 marionette=None, context_chrome=True):
        self._devicemanager = deviceManager
        self._appName = appName
        self._remoteProfile = None
        self._remoteLog = remoteLog
        self.marionette = marionette
        self.context_chrome = context_chrome
        self._is_emulator = False
        self.test_script = None
        self.test_script_args = None

        # Default our product to b2g
        self._product = "b2g"
        self.lastTestSeen = "b2gautomation.py"
        # Default log finish to mochitest standard
        self.logFinish = 'INFO SimpleTest FINISHED' 
        Automation.__init__(self)

    def setEmulator(self, is_emulator):
        self._is_emulator = is_emulator

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

    def installExtension(self, extensionSource, profileDir, extensionID=None):
        # Bug 827504 - installing special-powers extension separately causes problems in B2G
        if extensionID != "special-powers@mozilla.org":
            Automation.installExtension(self, extensionSource, profileDir, extensionID)

    # Set up what we need for the remote environment
    def environment(self, env=None, xrePath=None, crashreporter=True):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        # We always hide the results table in B2G; it's much slower if we don't.
        env['MOZ_HIDE_RESULTS_TABLE'] = '1'
        return env

    def waitForNet(self): 
        active = False
        time_out = 0
        while not active and time_out < 40:
            data = self._devicemanager._runCmd(['shell', '/system/bin/netcfg']).stdout.readlines()
            data.pop(0)
            for line in data:
                if (re.search(r'UP\s+(?:[0-9]{1,3}\.){3}[0-9]{1,3}', line)):
                    active = True
                    break
            time_out += 1
            time.sleep(1)
        return active

    def checkForCrashes(self, directory, symbolsPath):
        # XXX: This will have to be updated after crash reporting on b2g
        # is in place.
        dumpDir = tempfile.mkdtemp()
        self._devicemanager.getDirectory(self._remoteProfile + '/minidumps/', dumpDir)
        crashed = automationutils.checkForCrashes(dumpDir, symbolsPath, self.lastTestSeen)
        try:
          shutil.rmtree(dumpDir)
        except:
          print "WARNING: unable to remove directory: %s" % (dumpDir)
        return crashed

    def initializeProfile(self, profileDir, extraPrefs = [], useServerLocations = False):
        # add b2g specific prefs
        extraPrefs.extend(["browser.manifestURL='dummy (bug 772307)'"])
        return Automation.initializeProfile(self, profileDir, extraPrefs, useServerLocations)

    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        # if remote profile is specified, use that instead
        if (self._remoteProfile):
            profileDir = self._remoteProfile

        cmd, args = Automation.buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs)

        return app, args

    def getLanIp(self):
        nettools = NetworkTools()
        return nettools.getLanIp()

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime,
                      debuggerInfo, symbolsPath):
        """ Wait for tests to finish (as evidenced by a signature string
            in logcat), or for a given amount of time to elapse with no
            output.
        """
        timeout = timeout or 120
        responseDueBy = time.time() + timeout
        while True:
            currentlog = proc.stdout
            if currentlog:
                responseDueBy = time.time() + timeout
                print currentlog
                # Match the test filepath from the last TEST-START line found in the new
                # log content. These lines are in the form:
                # ... INFO TEST-START | /filepath/we/wish/to/capture.html\n
                testStartFilenames = re.findall(r"TEST-START \| ([^\s]*)", currentlog)
                if testStartFilenames:
                    self.lastTestSeen = testStartFilenames[-1]
                if hasattr(self, 'logFinish') and self.logFinish in currentlog:
                    return 0
            else:
                if time.time() > responseDueBy:
                    self.log.info("TEST-UNEXPECTED-FAIL | %s | application timed "
                                  "out after %d seconds with no output",
                                  self.lastTestSeen, int(timeout))
                    return 1

    def getDeviceStatus(self, serial=None):
        # Get the current status of the device.  If we know the device
        # serial number, we look for that, otherwise we use the (presumably
        # only) device shown in 'adb devices'.
        serial = serial or self._devicemanager.deviceSerial
        status = 'unknown'

        for line in self._devicemanager._runCmd(['devices']).stdout.readlines():
            result =  re.match('(.*?)\t(.*)', line)
            if result:
                thisSerial = result.group(1)
                if not serial or thisSerial == serial:
                    serial = thisSerial
                    status = result.group(2)

        return (serial, status)

    def restartB2G(self):
        # TODO hangs in subprocess.Popen without this delay
        time.sleep(5)
        self._devicemanager._checkCmd(['shell', 'stop', 'b2g'])
        # Wait for a bit to make sure B2G has completely shut down.
        time.sleep(10)
        self._devicemanager._checkCmd(['shell', 'start', 'b2g'])
        if self._is_emulator:
            self.marionette.emulator.wait_for_port()

    def rebootDevice(self):
        # find device's current status and serial number
        serial, status = self.getDeviceStatus()

        # reboot!
        self._devicemanager._runCmd(['shell', '/system/bin/reboot'])

        # The above command can return while adb still thinks the device is
        # connected, so wait a little bit for it to disconnect from adb.
        time.sleep(10)

        # wait for device to come back to previous status
        print 'waiting for device to come back online after reboot'
        start = time.time()
        rserial, rstatus = self.getDeviceStatus(serial)
        while rstatus != 'device':
            if time.time() - start > 120:
                # device hasn't come back online in 2 minutes, something's wrong
                raise Exception("Device %s (status: %s) not back online after reboot" % (serial, rstatus))
            time.sleep(5)
            rserial, rstatus = self.getDeviceStatus(serial)
        print 'device:', serial, 'status:', rstatus

    def Process(self, cmd, stdout=None, stderr=None, env=None, cwd=None):
        # On a desktop or fennec run, the Process method invokes a gecko
        # process in which to the tests.  For B2G, we simply
        # reboot the device (which was configured with a test profile
        # already), wait for B2G to start up, and then navigate to the
        # test url using Marionette.  There doesn't seem to be any way
        # to pass env variables into the B2G process, but this doesn't
        # seem to matter.

        # reboot device so it starts up with the mochitest profile
        # XXX:  We could potentially use 'stop b2g' + 'start b2g' to achieve
        # a similar effect; will see which is more stable while attempting
        # to bring up the continuous integration.
        if not self._is_emulator:
            self.rebootDevice()
            time.sleep(5)
            #wait for wlan to come up 
            if not self.waitForNet():
                raise Exception("network did not come up, please configure the network" + 
                                " prior to running before running the automation framework")

        # stop b2g
        self._devicemanager._runCmd(['shell', 'stop', 'b2g'])
        time.sleep(5)

        # relaunch b2g inside b2g instance
        instance = self.B2GInstance(self._devicemanager)

        time.sleep(5)

        # Set up port forwarding again for Marionette, since any that
        # existed previously got wiped out by the reboot.
        if not self._is_emulator:
            self._devicemanager._checkCmd(['forward',
                                           'tcp:%s' % self.marionette.port,
                                           'tcp:%s' % self.marionette.port])

        if self._is_emulator:
            self.marionette.emulator.wait_for_port()
        else:
            time.sleep(5)

        # start a marionette session
        session = self.marionette.start_session()
        if 'b2g' not in session:
            raise Exception("bad session value %s returned by start_session" % session)

        if self._is_emulator:
            # Disable offline status management (bug 777145), otherwise the network
            # will be 'offline' when the mochitests start.  Presumably, the network
            # won't be offline on a real device, so we only do this for emulators.
            self.marionette.set_context(self.marionette.CONTEXT_CHROME)
            self.marionette.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                Services.io.manageOfflineStatus = false;
                Services.io.offline = false;
                """)

        if self.context_chrome:
            self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        else:
            self.marionette.set_context(self.marionette.CONTEXT_CONTENT)

        # run the script that starts the tests
        if self.test_script:
            if os.path.isfile(self.test_script):
                script = open(self.test_script, 'r')
                self.marionette.execute_script(script.read(), script_args=self.test_script_args)
                script.close()
            elif isinstance(self.test_script, basestring):
                self.marionette.execute_script(self.test_script, script_args=self.test_script_args)
        else:
            # assumes the tests are started on startup automatically
            pass

        return instance

    # be careful here as this inner class doesn't have access to outer class members
    class B2GInstance(object):
        """Represents a B2G instance running on a device, and exposes
           some process-like methods/properties that are expected by the
           automation.
        """

        def __init__(self, dm):
            self.dm = dm
            self.stdout_proc = None
            self.queue = Queue.Queue()

            # Launch b2g in a separate thread, and dump all output lines
            # into a queue.  The lines in this queue are
            # retrieved and returned by accessing the stdout property of
            # this class.
            cmd = [self.dm._adbPath]
            if self.dm._deviceSerial:
                cmd.extend(['-s', self.dm._deviceSerial])
            cmd.append('shell')
            cmd.append('/system/bin/b2g.sh')
            proc = threading.Thread(target=self._save_stdout_proc, args=(cmd, self.queue))
            proc.daemon = True
            proc.start()

        def _save_stdout_proc(self, cmd, queue):
            self.stdout_proc = StdOutProc(cmd, queue)
            self.stdout_proc.run()
            if hasattr(self.stdout_proc, 'processOutput'):
                self.stdout_proc.processOutput()
            self.stdout_proc.waitForFinish()
            self.stdout_proc = None

        @property
        def pid(self):
            # a dummy value to make the automation happy
            return 0

        @property
        def stdout(self):
            # Return any lines in the queue used by the
            # b2g process handler.
            lines = []
            while True:
                try:
                    lines.append(self.queue.get_nowait())
                except Queue.Empty:
                    break
            return '\n'.join(lines)

        def wait(self, timeout = None):
            # this should never happen
            raise Exception("'wait' called on B2GInstance")

        def kill(self):
            # this should never happen
            raise Exception("'kill' called on B2GInstance")
