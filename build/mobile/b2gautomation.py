# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import automationutils
import threading
import os
import Queue
import re
import socket
import shutil
import sys
import tempfile
import time

from automation import Automation
from devicemanager import DeviceManager, NetworkTools
from mozprocess import ProcessHandlerMixin


class LogcatProc(ProcessHandlerMixin):
    """Process handler for logcat which puts all output in a Queue.
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
                 marionette=None):
        self._devicemanager = deviceManager
        self._appName = appName
        self._remoteProfile = None
        self._remoteLog = remoteLog
        self.marionette = marionette
        self._is_emulator = False

        # Default our product to b2g
        self._product = "b2g"
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

    # Set up what we need for the remote environment
    def environment(self, env=None, xrePath=None, crashreporter=True):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        # We always hide the results table in B2G; it's much slower if we don't.
        env['MOZ_HIDE_RESULTS_TABLE'] = '1'
        return env

    def checkForCrashes(self, directory, symbolsPath):
        # XXX: This will have to be updated after crash reporting on b2g
        # is in place.
        dumpDir = tempfile.mkdtemp()
        self._devicemanager.getDirectory(self._remoteProfile + '/minidumps/', dumpDir)
        automationutils.checkForCrashes(dumpDir, symbolsPath, self.lastTestSeen)
        try:
          shutil.rmtree(dumpDir)
        except:
          print "WARNING: unable to remove directory: %s" % (dumpDir)

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
                      debuggerInfo, symbolsPath, logger):
        """ Wait for mochitest to finish (as evidenced by a signature string
            in logcat), or for a given amount of time to elapse with no
            output.
        """
        timeout = timeout or 120

        didTimeout = False

        done = time.time() + timeout
        while True:
            currentlog = proc.stdout
            if currentlog:
                done = time.time() + timeout
                print currentlog
                if 'INFO SimpleTest FINISHED' in currentlog:
                    return 0
            else:
                if time.time() > done:
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

        for line in self._devicemanager.runCmd(['devices']).stdout.readlines():
            result =  re.match('(.*?)\t(.*)', line)
            if result:
                thisSerial = result.group(1)
                if not serial or thisSerial == serial:
                    serial = thisSerial
                    status = result.group(2)

        return (serial, status)

    def restartB2G(self):
        self._devicemanager.checkCmd(['shell', 'stop', 'b2g'])
        # Wait for a bit to make sure B2G has completely shut down.
        time.sleep(5)
        self._devicemanager.checkCmd(['shell', 'start', 'b2g'])
        if self._is_emulator:
            self.marionette.emulator.wait_for_port()

    def rebootDevice(self):
        # find device's current status and serial number
        serial, status = self.getDeviceStatus()

        # reboot!
        self._devicemanager.checkCmd(['reboot'])

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
        # process in which to run mochitests.  For B2G, we simply
        # reboot the device (which was configured with a test profile
        # already), wait for B2G to start up, and then navigate to the
        # test url using Marionette.  There doesn't seem to be any way
        # to pass env variables into the B2G process, but this doesn't 
        # seem to matter.

        instance = self.B2GInstance(self._devicemanager)

        # reboot device so it starts up with the mochitest profile
        # XXX:  We could potentially use 'stop b2g' + 'start b2g' to achieve
        # a similar effect; will see which is more stable while attempting
        # to bring up the continuous integration.
        if self._is_emulator:
            self.restartB2G()
        else:
            self.rebootDevice()

        # Infrequently, gecko comes up before networking does, so wait a little
        # bit to give the network time to become available.
        # XXX:  need a more robust mechanism for this
        time.sleep(20)

        # Set up port forwarding again for Marionette, since any that
        # existed previously got wiped out by the reboot.
        if not self._is_emulator:
            self._devicemanager.checkCmd(['forward',
                                          'tcp:%s' % self.marionette.port,
                                          'tcp:%s' % self.marionette.port])

        # start a marionette session
        session = self.marionette.start_session()
        if 'b2g' not in session:
            raise Exception("bad session value %s returned by start_session" % session)

        # start the tests by navigating to the mochitest url
        self.marionette.execute_script("window.location.href='%s';" % self.testURL)

        return instance

    # be careful here as this inner class doesn't have access to outer class members
    class B2GInstance(object):
        """Represents a B2G instance running on a device, and exposes
           some process-like methods/properties that are expected by the
           automation.
        """

        def __init__(self, dm):
            self.dm = dm
            self.logcat_proc = None
            self.queue = Queue.Queue()

            # Launch logcat in a separate thread, and dump all output lines
            # into a queue.  The lines in this queue are
            # retrieved and returned by accessing the stdout property of
            # this class.
            cmd = [self.dm.adbPath]
            if self.dm.deviceSerial:
                cmd.extend(['-s', self.dm.deviceSerial])
            cmd.append('logcat')
            proc = threading.Thread(target=self._save_logcat_proc, args=(cmd, self.queue))
            proc.daemon = True
            proc.start()

        def _save_logcat_proc(self, cmd, queue):
            self.logcat_proc = LogcatProc(cmd, queue)
            self.logcat_proc.run()
            self.logcat_proc.waitForFinish()
            self.logcat_proc = None

        @property
        def pid(self):
            # a dummy value to make the automation happy
            return 0

        @property
        def stdout(self):
            # Return any lines in the queue used by the
            # logcat process handler.
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

