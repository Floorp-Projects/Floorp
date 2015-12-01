# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import os
import sys
import tempfile
import traceback

# We need to know our current directory so that we can serve our test files from it.
here = os.path.abspath(os.path.dirname(__file__))
if here not in sys.path:
    sys.path.insert(0, here)

from automation import Automation
from b2gautomation import B2GRemoteAutomation
from runreftestmulet import run_test_harness as run_mulet
from remotereftest import RemoteReftestResolver, ReftestServer
from runreftest import RefTest
import reftestcommandline

from mozdevice import DeviceManagerADB, DMError
from marionette import Marionette

class ProfileConfigParser(ConfigParser.RawConfigParser):
    """Subclass of RawConfigParser that outputs .ini files in the exact
       format expected for profiles.ini, which is slightly different
       than the default format.
    """

    def optionxform(self, optionstr):
        return optionstr

    def write(self, fp):
        if self._defaults:
            fp.write("[%s]\n" % ConfigParser.DEFAULTSECT)
            for (key, value) in self._defaults.items():
                fp.write("%s=%s\n" % (key, str(value).replace('\n', '\n\t')))
            fp.write("\n")
        for section in self._sections:
            fp.write("[%s]\n" % section)
            for (key, value) in self._sections[section].items():
                if key == "__name__":
                    continue
                if (value is not None) or (self._optcre == self.OPTCRE):
                    key = "=".join((key, str(value).replace('\n', '\n\t')))
                fp.write("%s\n" % (key))
            fp.write("\n")

class B2GRemoteReftest(RefTest):

    _devicemanager = None
    localProfile = None
    remoteApp = ''
    profile = None
    resolver_cls = RemoteReftestResolver

    def __init__(self, automation, devicemanager, options, scriptDir):
        RefTest.__init__(self)
        self.automation = automation
        self._devicemanager = devicemanager
        self.runSSLTunnel = False
        self.remoteTestRoot = options.remoteTestRoot
        self.remoteProfile = options.remoteProfile
        self.automation.setRemoteProfile(self.remoteProfile)
        self.localLogName = options.localLogName
        self.remoteLogFile = options.remoteLogFile
        self.bundlesDir = '/system/b2g/distribution/bundles'
        self.remoteMozillaPath = '/data/b2g/mozilla'
        self.remoteProfilesIniPath = os.path.join(self.remoteMozillaPath, 'profiles.ini')
        self.originalProfilesIni = None
        self.scriptDir = scriptDir
        self.SERVER_STARTUP_TIMEOUT = 90
        if self.automation.IS_DEBUG_BUILD:
            self.SERVER_STARTUP_TIMEOUT = 180

    def cleanup(self, profileDir):
        # Pull results back from device
        if (self.remoteLogFile):
            try:
                self._devicemanager.getFile(self.remoteLogFile, self.localLogName)
            except:
                print "ERROR: We were not able to retrieve the info from %s" % self.remoteLogFile
                sys.exit(5)

        # Delete any bundled extensions
        if profileDir:
            extensionDir = os.path.join(profileDir, 'extensions', 'staged')
            for filename in os.listdir(extensionDir):
                try:
                    self._devicemanager._checkCmd(['shell', 'rm', '-rf',
                                                     os.path.join(self.bundlesDir, filename)])
                except DMError:
                    pass

        # Restore the original profiles.ini.
        if self.originalProfilesIni:
            try:
                if not self.automation._is_emulator:
                    self.restoreProfilesIni()
                os.remove(self.originalProfilesIni)
            except:
                pass

        if not self.automation._is_emulator:
            self._devicemanager.removeFile(self.remoteLogFile)
            self._devicemanager.removeDir(self.remoteProfile)
            self._devicemanager.removeDir(self.remoteTestRoot)

            # We've restored the original profile, so reboot the device so that
            # it gets picked up.
            self.automation.rebootDevice()

        RefTest.cleanup(self, profileDir)
        if getattr(self, 'pidFile', '') != '':
            try:
                os.remove(self.pidFile)
                os.remove(self.pidFile + ".xpcshell.pid")
            except:
                print "Warning: cleaning up pidfile '%s' was unsuccessful from the test harness" % self.pidFile

    def findPath(self, paths, filename = None):
        for path in paths:
            p = path
            if filename:
                p = os.path.join(p, filename)
            if os.path.exists(self.getFullPath(p)):
                return path
        return None

    def startWebServer(self, options):
        """ Create the webserver on the host and start it up """
        remoteXrePath = options.xrePath
        remoteProfilePath = self.remoteProfile
        remoteUtilityPath = options.utilityPath
        localAutomation = Automation()
        localAutomation.IS_WIN32 = False
        localAutomation.IS_LINUX = False
        localAutomation.IS_MAC = False
        localAutomation.UNIXISH = False
        hostos = sys.platform
        if hostos in ['mac', 'darwin']:
            localAutomation.IS_MAC = True
        elif hostos in ['linux', 'linux2']:
            localAutomation.IS_LINUX = True
            localAutomation.UNIXISH = True
        elif hostos in ['win32', 'win64']:
            localAutomation.BIN_SUFFIX = ".exe"
            localAutomation.IS_WIN32 = True

        paths = [options.xrePath,
                 localAutomation.DIST_BIN,
                 self.automation._product,
                 os.path.join('..', self.automation._product)]
        options.xrePath = self.findPath(paths)
        if options.xrePath == None:
            print "ERROR: unable to find xulrunner path for %s, please specify with --xre-path" % (os.name)
            sys.exit(1)
        paths.append("bin")
        paths.append(os.path.join("..", "bin"))

        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"

        options.utilityPath = self.findPath(paths, xpcshell)
        if options.utilityPath == None:
            print "ERROR: unable to find utility path for %s, please specify with --utility-path" % (os.name)
            sys.exit(1)

        xpcshell = os.path.join(options.utilityPath, xpcshell)
        if self.automation.elf_arm(xpcshell):
            raise Exception('xpcshell at %s is an ARM binary; please use '
                            'the --utility-path argument to specify the path '
                            'to a desktop version.' % xpcshell)

        options.serverProfilePath = tempfile.mkdtemp()
        self.server = ReftestServer(localAutomation, options, self.scriptDir)
        retVal = self.server.start()
        if retVal:
            return retVal

        if (options.pidFile != ""):
            f = open(options.pidFile + ".xpcshell.pid", 'w')
            f.write("%s" % self.server._process.pid)
            f.close()

        retVal = self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)
        if retVal:
            return retVal

        options.xrePath = remoteXrePath
        options.utilityPath = remoteUtilityPath
        options.profilePath = remoteProfilePath
        return 0

    def stopWebServer(self, options):
        if hasattr(self, 'server'):
            self.server.stop()

    def restoreProfilesIni(self):
        # restore profiles.ini on the device to its previous state
        if not self.originalProfilesIni or not os.access(self.originalProfilesIni, os.F_OK):
            raise DMError('Unable to install original profiles.ini; file not found: %s',
                          self.originalProfilesIni)

        self._devicemanager.pushFile(self.originalProfilesIni, self.remoteProfilesIniPath)

    def updateProfilesIni(self, profilePath):
        # update profiles.ini on the device to point to the test profile
        self.originalProfilesIni = tempfile.mktemp()
        self._devicemanager.getFile(self.remoteProfilesIniPath, self.originalProfilesIni)

        config = ProfileConfigParser()
        config.read(self.originalProfilesIni)
        for section in config.sections():
            if 'Profile' in section:
                config.set(section, 'IsRelative', 0)
                config.set(section, 'Path', profilePath)

        newProfilesIni = tempfile.mktemp()
        with open(newProfilesIni, 'wb') as configfile:
            config.write(configfile)

        self._devicemanager.pushFile(newProfilesIni, self.remoteProfilesIniPath)
        try:
            os.remove(newProfilesIni)
        except:
            pass


    def createReftestProfile(self, options, manifests):
        profile = RefTest.createReftestProfile(self, options, manifests,
                                               server=options.remoteWebServer)
        profileDir = profile.profile

        prefs = {}

        # Turn off the locale picker screen
        prefs["browser.firstrun.show.localepicker"] = False
        prefs["b2g.system_startup_url"] = "app://test-container.gaiamobile.org/index.html"
        prefs["b2g.system_manifest_url"] = "app://test-container.gaiamobile.org/manifest.webapp"
        prefs["dom.ipc.tabs.disabled"] = False
        prefs["dom.mozBrowserFramesEnabled"] = True
        prefs["font.size.inflation.emPerLine"] = 0
        prefs["font.size.inflation.minTwips"] = 0
        prefs["network.dns.localDomains"] = "app://test-container.gaiamobile.org"
        prefs["reftest.browser.iframe.enabled"] = False
        prefs["reftest.remote"] = True

        # Set a future policy version to avoid the telemetry prompt.
        prefs["toolkit.telemetry.prompted"] = 999
        prefs["toolkit.telemetry.notifiedOptOut"] = 999
        # Make sure we disable system updates
        prefs["app.update.enabled"] = False
        prefs["app.update.url"] = ""
        prefs["app.update.url.override"] = ""
        # Disable webapp updates
        prefs["webapps.update.enabled"] = False
        # Disable tiles also
        prefs["browser.newtabpage.directory.source"] = ""
        prefs["browser.newtabpage.directory.ping"] = ""

        if options.oop:
            prefs['browser.tabs.remote.autostart'] = True
            prefs['reftest.browser.iframe.enabled'] = True

        # Set the extra prefs.
        profile.set_preferences(prefs)

        # Copy the profile to the device.
        self._devicemanager.removeDir(self.remoteProfile)
        try:
            self._devicemanager.pushDir(profileDir, self.remoteProfile)
        except DMError:
            print "Automation Error: Unable to copy profile to device."
            raise

        # Copy the extensions to the B2G bundles dir.
        extensionDir = os.path.join(profileDir, 'extensions', 'staged')
        # need to write to read-only dir
        self._devicemanager._checkCmd(['remount'])
        for filename in os.listdir(extensionDir):
            self._devicemanager._checkCmd(['shell', 'rm', '-rf',
                                             os.path.join(self.bundlesDir, filename)])
        try:
            self._devicemanager.pushDir(extensionDir, self.bundlesDir)
        except DMError:
            print "Automation Error: Unable to copy extensions to device."
            raise

        self.updateProfilesIni(self.remoteProfile)

        options.profilePath = self.remoteProfile
        return profile

    def copyExtraFilesToProfile(self, options, profile):
        profileDir = profile.profile
        RefTest.copyExtraFilesToProfile(self, options, profile)
        try:
            self._devicemanager.pushDir(profileDir, options.remoteProfile)
        except DMError:
            print "Automation Error: Failed to copy extra files to device"
            raise

    def environment(self, **kwargs):
     return self.automation.environment(**kwargs)

    def runApp(self, profile, binary, cmdargs, env,
               timeout=None, debuggerInfo=None,
               symbolsPath=None, options=None,
               valgrindPath=None, valgrindArgs=None, valgrindSuppFiles=None):
        status = self.automation.runApp(None, env,
                                        binary,
                                        profile.profile,
                                        cmdargs,
                                        utilityPath=options.utilityPath,
                                        xrePath=options.xrePath,
                                        debuggerInfo=debuggerInfo,
                                        symbolsPath=symbolsPath,
                                        timeout=timeout)
        return status


def run_remote_reftests(parser, options):
    auto = B2GRemoteAutomation(None, "fennec", context_chrome=True)

    # create our Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        auto.setEmulator(True)
        if options.noWindow:
            kwargs['noWindow'] = True
        if options.geckoPath:
            kwargs['gecko_path'] = options.geckoPath
        if options.logdir:
            kwargs['logdir'] = options.logdir
        if options.busybox:
            kwargs['busybox'] = options.busybox
        if options.symbolsPath:
            kwargs['symbols_path'] = options.symbolsPath
    if options.emulator_res:
        kwargs['emulator_res'] = options.emulator_res
    if options.b2gPath:
        kwargs['homedir'] = options.b2gPath
    if options.marionette:
        host,port = options.marionette.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)
    if options.adb_path:
        kwargs['adb_path'] = options.adb_path
    marionette = Marionette(**kwargs)
    auto.marionette = marionette

    if options.emulator:
        dm = marionette.emulator.dm
    else:
        # create the DeviceManager
        kwargs = {'adbPath': options.adb_path,
                  'deviceRoot': options.remoteTestRoot}
        if options.deviceIP:
            kwargs.update({'host': options.deviceIP,
                           'port': options.devicePort})
        dm = DeviceManagerADB(**kwargs)
    auto.setDeviceManager(dm)

    parser.validate_remote(options, auto)

    # TODO fix exception
    if not options.ignoreWindowSize:
        parts = dm.getInfo('screen')['screen'][0].split()
        width = int(parts[0].split(':')[1])
        height = int(parts[1].split(':')[1])
        if (width < 1366 or height < 1050):
            print "ERROR: Invalid screen resolution %sx%s, please adjust to 1366x1050 or higher" % (width, height)
            return 1

    auto.setProduct("b2g")
    auto.test_script = os.path.join(here, 'b2g_start_script.js')
    auto.test_script_args = [options.remoteWebServer, options.httpPort]
    auto.logFinish = "REFTEST TEST-START | Shutdown"

    reftest = B2GRemoteReftest(auto, dm, options, here)
    parser.validate(options, reftest)

    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent);
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)

    # Hack in a symbolic link for jsreftest
    os.system("ln -s %s %s" % (os.path.join('..', 'jsreftest'), os.path.join(here, 'jsreftest')))


    # Start the webserver
    retVal = 1
    try:
        retVal = reftest.startWebServer(options)
        if retVal:
            return retVal
        procName = options.app.split('/')[-1]
        if (dm.processExist(procName)):
            dm.killProcess(procName)

        retVal = reftest.runTests(options.tests, options)
    except:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        reftest.stopWebServer(options)
        try:
            reftest.cleanup(None)
        except:
            pass
        return 1

    reftest.stopWebServer(options)
    return retVal

def run_remote(**kwargs):
    # Tests need to be served from a subdirectory of the server. Symlink
    # topsrcdir here to get around this.
    parser = reftestcommandline.B2GArgumentParser()
    parser.set_defaults(**kwargs)
    options = parser.parse_args(kwargs["tests"])
    return run_remote_reftests(parser, options)

def main():
    parser = reftestcommandline.B2GArgumentParser()
    options = parser.parse_args()

    if options.mulet:
        return run_mulet(parser, options)
    return run_remote_reftests(parser, options)


if __name__ == "__main__":
    sys.exit(main())

