# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import os
import sys
import tempfile
import traceback

# We need to know our current directory so that we can serve our test files from it.
SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))
sys.path.insert(0, SCRIPT_DIRECTORY)

from automation import Automation
from b2gautomation import B2GRemoteAutomation
from runreftest import RefTest
from runreftest import ReftestOptions
from remotereftest import ReftestServer

from mozdevice import DeviceManagerADB, DMError
from marionette import Marionette


class B2GOptions(ReftestOptions):

    def __init__(self, automation, **kwargs):
        defaults = {}
        ReftestOptions.__init__(self, automation)

        self.add_option("--b2gpath", action="store",
                    type = "string", dest = "b2gPath",
                    help = "path to B2G repo or qemu dir")
        defaults["b2gPath"] = None

        self.add_option("--marionette", action="store",
                    type = "string", dest = "marionette",
                    help = "host:port to use when connecting to Marionette")
        defaults["marionette"] = None

        self.add_option("--emulator", action="store",
                    type="string", dest = "emulator",
                    help = "Architecture of emulator to use: x86 or arm")
        defaults["emulator"] = None
        self.add_option("--emulator-res", action="store",
                    type="string", dest = "emulator_res",
                    help = "Emulator resolution of the format '<width>x<height>'")
        defaults["emulator_res"] = None

        self.add_option("--no-window", action="store_true",
                    dest = "noWindow",
                    help = "Pass --no-window to the emulator")
        defaults["noWindow"] = False

        self.add_option("--adbpath", action="store",
                    type = "string", dest = "adbPath",
                    help = "path to adb")
        defaults["adbPath"] = "adb"

        self.add_option("--deviceIP", action="store",
                    type = "string", dest = "deviceIP",
                    help = "ip address of remote device to test")
        defaults["deviceIP"] = None

        self.add_option("--devicePort", action="store",
                    type = "string", dest = "devicePort",
                    help = "port of remote device to test")
        defaults["devicePort"] = 20701

        self.add_option("--remote-logfile", action="store",
                    type = "string", dest = "remoteLogFile",
                    help = "Name of log file on the device relative to the device root.  PLEASE ONLY USE A FILENAME.")
        defaults["remoteLogFile"] = None

        self.add_option("--remote-webserver", action = "store",
                    type = "string", dest = "remoteWebServer",
                    help = "ip address where the remote web server is hosted at")
        defaults["remoteWebServer"] = None

        self.add_option("--http-port", action = "store",
                    type = "string", dest = "httpPort",
                    help = "ip address where the remote web server is hosted at")
        defaults["httpPort"] = automation.DEFAULT_HTTP_PORT

        self.add_option("--ssl-port", action = "store",
                    type = "string", dest = "sslPort",
                    help = "ip address where the remote web server is hosted at")
        defaults["sslPort"] = automation.DEFAULT_SSL_PORT

        self.add_option("--pidfile", action = "store",
                    type = "string", dest = "pidFile",
                    help = "name of the pidfile to generate")
        defaults["pidFile"] = ""
        self.add_option("--gecko-path", action="store",
                        type="string", dest="geckoPath",
                        help="the path to a gecko distribution that should "
                        "be installed on the emulator prior to test")
        defaults["geckoPath"] = None
        self.add_option("--logcat-dir", action="store",
                        type="string", dest="logcat_dir",
                        help="directory to store logcat dump files")
        defaults["logcat_dir"] = None
        self.add_option('--busybox', action='store',
                        type='string', dest='busybox',
                        help="Path to busybox binary to install on device")
        defaults['busybox'] = None
        self.add_option("--httpd-path", action = "store",
                    type = "string", dest = "httpdPath",
                    help = "path to the httpd.js file")
        defaults["httpdPath"] = None
        defaults["remoteTestRoot"] = "/data/local/tests"
        defaults["logFile"] = "reftest.log"
        defaults["autorun"] = True
        defaults["closeWhenDone"] = True
        defaults["testPath"] = ""

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        if not options.remoteTestRoot:
            options.remoteTestRoot = self._automation._devicemanager.getDeviceRoot() + "/reftest"
        options.remoteProfile = options.remoteTestRoot + "/profile"

        productRoot = options.remoteTestRoot + "/" + self._automation._product
        if options.utilityPath == self._automation.DIST_BIN:
            options.utilityPath = productRoot + "/bin"

        if options.remoteWebServer == None:
            if os.name != "nt":
                options.remoteWebServer = self._automation.getLanIp()
            else:
                print "ERROR: you must specify a --remote-webserver=<ip address>\n"
                return None

        options.webServer = options.remoteWebServer

        if options.geckoPath and not options.emulator:
            self.error("You must specify --emulator if you specify --gecko-path")

        if options.logcat_dir and not options.emulator:
            self.error("You must specify --emulator if you specify --logcat-dir")

        #if not options.emulator and not options.deviceIP:
        #    print "ERROR: you must provide a device IP"
        #    return None

        if options.remoteLogFile == None:
            options.remoteLogFile = "reftest.log"

        options.localLogName = options.remoteLogFile
        options.remoteLogFile = options.remoteTestRoot + '/' + options.remoteLogFile

        # Ensure that the options.logfile (which the base class uses) is set to
        # the remote setting when running remote. Also, if the user set the
        # log file name there, use that instead of reusing the remotelogfile as above.
        if (options.logFile):
            # If the user specified a local logfile name use that
            options.localLogName = options.logFile
        options.logFile = options.remoteLogFile

        # Only reset the xrePath if it wasn't provided
        if options.xrePath == None:
            options.xrePath = options.utilityPath
        options.xrePath = os.path.abspath(options.xrePath)

        if options.pidFile != "":
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # httpd-path is specified by standard makefile targets and may be specified
        # on the command line to select a particular version of httpd.js. If not
        # specified, try to select the one from from the xre bundle, as required in bug 882932.
        if not options.httpdPath:
            options.httpdPath = os.path.join(options.xrePath, "components")

        return options


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


class B2GReftest(RefTest):

    _automation = None
    _devicemanager = None
    localProfile = None
    remoteApp = ''
    profile = None

    def __init__(self, automation, devicemanager, options, scriptDir):
        self._automation = automation
        RefTest.__init__(self, self._automation)
        self._devicemanager = devicemanager
        self.runSSLTunnel = False
        self.remoteTestRoot = options.remoteTestRoot
        self.remoteProfile = options.remoteProfile
        self._automation.setRemoteProfile(self.remoteProfile)
        self.localLogName = options.localLogName
        self.remoteLogFile = options.remoteLogFile
        self.bundlesDir = '/system/b2g/distribution/bundles'
        self.userJS = '/data/local/user.js'
        self.remoteMozillaPath = '/data/b2g/mozilla'
        self.remoteProfilesIniPath = os.path.join(self.remoteMozillaPath, 'profiles.ini')
        self.originalProfilesIni = None
        self.scriptDir = scriptDir
        self.SERVER_STARTUP_TIMEOUT = 90
        if self._automation.IS_DEBUG_BUILD:
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
        extensionDir = os.path.join(profileDir, 'extensions', 'staged')
        for filename in os.listdir(extensionDir):
            try:
                self._devicemanager._checkCmdAs(['shell', 'rm', '-rf',
                                                 os.path.join(self.bundlesDir, filename)])
            except DMError:
                pass

        # Restore the original profiles.ini.
        if self.originalProfilesIni:
            try:
                if not self._automation._is_emulator:
                    self.restoreProfilesIni()
                os.remove(self.originalProfilesIni)
            except:
                pass

        if not self._automation._is_emulator:
            self._devicemanager.removeFile(self.remoteLogFile)
            self._devicemanager.removeDir(self.remoteProfile)
            self._devicemanager.removeDir(self.remoteTestRoot)

            # Restore the original user.js.
            self._devicemanager._checkCmdAs(['shell', 'rm', '-f', self.userJS])
            self._devicemanager._checkCmdAs(['shell', 'dd', 'if=%s.orig' % self.userJS, 'of=%s' % self.userJS])

            # We've restored the original profile, so reboot the device so that
            # it gets picked up.
            self._automation.rebootDevice()

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
                 self._automation._product,
                 os.path.join('..', self._automation._product)]
        options.xrePath = self.findPath(paths)
        if options.xrePath == None:
            print "ERROR: unable to find xulrunner path for %s, please specify with --xre-path" % (os.name)
            sys.exit(1)
        paths.append("bin")
        paths.append(os.path.join("..", "bin"))

        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"

        if (options.utilityPath):
            paths.insert(0, options.utilityPath)
        options.utilityPath = self.findPath(paths, xpcshell)
        if options.utilityPath == None:
            print "ERROR: unable to find utility path for %s, please specify with --utility-path" % (os.name)
            sys.exit(1)

        xpcshell = os.path.join(options.utilityPath, xpcshell)
        if self._automation.elf_arm(xpcshell):
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


    def createReftestProfile(self, options, profileDir, reftestlist):
        print "profileDir: " + str(profileDir)
        retVal = RefTest.createReftestProfile(self, options, profileDir, reftestlist, server=options.remoteWebServer)

        # Turn off the locale picker screen
        fhandle = open(os.path.join(profileDir, "user.js"), 'a')
        fhandle.write("""
user_pref("browser.firstrun.show.localepicker", false);
user_pref("browser.homescreenURL","app://system.gaiamobile.org");\n
user_pref("browser.manifestURL","app://system.gaiamobile.org/manifest.webapp");\n
user_pref("browser.tabs.remote", false);\n
user_pref("dom.ipc.browser_frames.oop_by_default", true);\n
user_pref("dom.ipc.tabs.disabled", false);\n
user_pref("dom.mozBrowserFramesEnabled", true);\n
user_pref("dom.mozBrowserFramesWhitelist","app://system.gaiamobile.org");\n
user_pref("network.dns.localDomains","app://system.gaiamobile.org");\n
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);
user_pref("reftest.browser.iframe.enabled", false);
user_pref("reftest.remote", true);
user_pref("reftest.uri", "%s");
// Set a future policy version to avoid the telemetry prompt.
user_pref("toolkit.telemetry.prompted", 999);
user_pref("toolkit.telemetry.notifiedOptOut", 999);
""" % reftestlist)

        #workaround for jsreftests.
        if getattr(options, 'enablePrivilege', False):
            fhandle.write("""
user_pref("capability.principal.codebase.p2.granted", "UniversalXPConnect");
user_pref("capability.principal.codebase.p2.id", "http://%s:%s");
""" % (options.remoteWebServer, options.httpPort))

        # Close the file
        fhandle.close()

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
        self._devicemanager._checkCmdAs(['remount'])
        for filename in os.listdir(extensionDir):
            self._devicemanager._checkCmdAs(['shell', 'rm', '-rf',
                                             os.path.join(self.bundlesDir, filename)])
        try:
            self._devicemanager.pushDir(extensionDir, self.bundlesDir)
        except DMError:
            print "Automation Error: Unable to copy extensions to device."
            raise

        # In B2G, user.js is always read from /data/local, not the profile
        # directory.  Backup the original user.js first so we can restore it.
        self._devicemanager._checkCmdAs(['shell', 'rm', '-f', '%s.orig' % self.userJS])
        self._devicemanager._checkCmdAs(['shell', 'dd', 'if=%s' % self.userJS, 'of=%s.orig' % self.userJS])
        self._devicemanager.pushFile(os.path.join(profileDir, "user.js"), self.userJS)

        self.updateProfilesIni(self.remoteProfile)

        options.profilePath = self.remoteProfile
        return retVal

    def copyExtraFilesToProfile(self, options, profileDir):
        RefTest.copyExtraFilesToProfile(self, options, profileDir)
        try:
            self._devicemanager.pushDir(profileDir, options.remoteProfile)
        except DMError:
            print "Automation Error: Failed to copy extra files to device"
            raise

    def getManifestPath(self, path):
        return path


def main(args=sys.argv[1:]):
    auto = B2GRemoteAutomation(None, "fennec", context_chrome=True)
    parser = B2GOptions(auto)
    options, args = parser.parse_args(args)

    # create our Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        auto.setEmulator(True)
        if options.noWindow:
            kwargs['noWindow'] = True
        if options.geckoPath:
            kwargs['gecko_path'] = options.geckoPath
        if options.logcat_dir:
            kwargs['logcat_dir'] = options.logcat_dir
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
    marionette = Marionette.getMarionetteOrExit(**kwargs)
    auto.marionette = marionette

    # create the DeviceManager
    kwargs = {'adbPath': options.adbPath,
              'deviceRoot': options.remoteTestRoot}
    if options.deviceIP:
        kwargs.update({'host': options.deviceIP,
                       'port': options.devicePort})
    dm = DeviceManagerADB(**kwargs)
    auto.setDeviceManager(dm)

    options = parser.verifyRemoteOptions(options)

    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        sys.exit(1)

    # TODO fix exception
    if not options.ignoreWindowSize:
        parts = dm.getInfo('screen')['screen'][0].split()
        width = int(parts[0].split(':')[1])
        height = int(parts[1].split(':')[1])
        if (width < 1366 or height < 1050):
            print "ERROR: Invalid screen resolution %sx%s, please adjust to 1366x1050 or higher" % (width, height)
            return 1

    auto.setProduct("b2g")
    auto.test_script = os.path.join(SCRIPT_DIRECTORY, 'b2g_start_script.js')
    auto.test_script_args = [options.remoteWebServer, options.httpPort]
    auto.logFinish = "REFTEST TEST-START | Shutdown"

    reftest = B2GReftest(auto, dm, options, SCRIPT_DIRECTORY)
    options = parser.verifyCommonOptions(options, reftest)

    logParent = os.path.dirname(options.remoteLogFile)
    dm.mkDir(logParent);
    auto.setRemoteLog(options.remoteLogFile)
    auto.setServerInfo(options.webServer, options.httpPort, options.sslPort)

    # Hack in a symbolic link for jsreftest
    os.system("ln -s %s %s" % (os.path.join('..', 'jsreftest'), os.path.join(SCRIPT_DIRECTORY, 'jsreftest')))

    # Dynamically build the reftest URL if possible, beware that args[0] should exist 'inside' the webroot
    manifest = args[0]
    if os.path.exists(os.path.join(SCRIPT_DIRECTORY, args[0])):
        manifest = "http://%s:%s/%s" % (options.remoteWebServer, options.httpPort, args[0])
    elif os.path.exists(args[0]):
        manifestPath = os.path.abspath(args[0]).split(SCRIPT_DIRECTORY)[1].strip('/')
        manifest = "http://%s:%s/%s" % (options.remoteWebServer, options.httpPort, manifestPath)
    else:
        print "ERROR: Could not find test manifest '%s'" % manifest
        return 1

    # Start the webserver
    retVal = 1
    try:
        retVal = reftest.startWebServer(options)
        if retVal:
            return retVal
        procName = options.app.split('/')[-1]
        if (dm.processExist(procName)):
            dm.killProcess(procName)

        cmdlineArgs = ["-reftest", manifest]
        if getattr(options, 'bootstrap', False):
            cmdlineArgs = []

        retVal = reftest.runTests(manifest, options, cmdlineArgs)
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

if __name__ == "__main__":
    sys.exit(main())

