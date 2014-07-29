# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import time
import tempfile
import traceback

# We need to know our current directory so that we can serve our test files from it.
SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))

from runreftest import RefTest
from runreftest import ReftestOptions
from automation import Automation
import devicemanager
import droid
import moznetwork
from remoteautomation import RemoteAutomation, fennecLogcatFilters

class RemoteOptions(ReftestOptions):
    def __init__(self, automation):
        ReftestOptions.__init__(self)
        self.automation = automation

        defaults = {}
        defaults["logFile"] = "reftest.log"
        # app, xrePath and utilityPath variables are set in main function
        defaults["app"] = ""
        defaults["xrePath"] = ""
        defaults["utilityPath"] = ""
        defaults["runTestsInParallel"] = False

        self.add_option("--remote-app-path", action="store",
                    type = "string", dest = "remoteAppPath",
                    help = "Path to remote executable relative to device root using only forward slashes.  Either this or app must be specified, but not both.")
        defaults["remoteAppPath"] = None

        self.add_option("--deviceIP", action="store",
                    type = "string", dest = "deviceIP",
                    help = "ip address of remote device to test")
        defaults["deviceIP"] = None

        self.add_option("--devicePort", action="store",
                    type = "string", dest = "devicePort",
                    help = "port of remote device to test")
        defaults["devicePort"] = 20701

        self.add_option("--remote-product-name", action="store",
                    type = "string", dest = "remoteProductName",
                    help = "Name of product to test - either fennec or firefox, defaults to fennec")
        defaults["remoteProductName"] = "fennec"

        self.add_option("--remote-webserver", action="store",
                    type = "string", dest = "remoteWebServer",
                    help = "IP Address of the webserver hosting the reftest content")
        defaults["remoteWebServer"] = moznetwork.get_ip()

        self.add_option("--http-port", action = "store",
                    type = "string", dest = "httpPort",
                    help = "port of the web server for http traffic")
        defaults["httpPort"] = automation.DEFAULT_HTTP_PORT

        self.add_option("--ssl-port", action = "store",
                    type = "string", dest = "sslPort",
                    help = "Port for https traffic to the web server")
        defaults["sslPort"] = automation.DEFAULT_SSL_PORT

        self.add_option("--remote-logfile", action="store",
                    type = "string", dest = "remoteLogFile",
                    help = "Name of log file on the device relative to device root.  PLEASE USE ONLY A FILENAME.")
        defaults["remoteLogFile"] = None

        self.add_option("--pidfile", action = "store",
                    type = "string", dest = "pidFile",
                    help = "name of the pidfile to generate")
        defaults["pidFile"] = ""

        self.add_option("--bootstrap", action="store_true", dest = "bootstrap",
                    help = "test with a bootstrap addon required for native Fennec")
        defaults["bootstrap"] = False

        self.add_option("--dm_trans", action="store",
                    type = "string", dest = "dm_trans",
                    help = "the transport to use to communicate with device: [adb|sut]; default=sut")
        defaults["dm_trans"] = "sut"

        self.add_option("--remoteTestRoot", action = "store",
                    type = "string", dest = "remoteTestRoot",
                    help = "remote directory to use as test root (eg. /mnt/sdcard/tests or /data/local/tests)")
        defaults["remoteTestRoot"] = None

        self.add_option("--httpd-path", action = "store",
                    type = "string", dest = "httpdPath",
                    help = "path to the httpd.js file")
        defaults["httpdPath"] = None

        defaults["localLogName"] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        if options.runTestsInParallel:
            self.error("Cannot run parallel tests here")

        # Ensure our defaults are set properly for everything we can infer
        if not options.remoteTestRoot:
            options.remoteTestRoot = self.automation._devicemanager.deviceRoot + '/reftest'
        options.remoteProfile = options.remoteTestRoot + "/profile"

        # Verify that our remotewebserver is set properly
        if (options.remoteWebServer == None or
            options.remoteWebServer == '127.0.0.1'):
            print "ERROR: Either you specified the loopback for the remote webserver or ",
            print "your local IP cannot be detected.  Please provide the local ip in --remote-webserver"
            return None

        # One of remoteAppPath (relative path to application) or the app (executable) must be
        # set, but not both.  If both are set, we destroy the user's selection for app
        # so instead of silently destroying a user specificied setting, we error.
        if (options.remoteAppPath and options.app):
            print "ERROR: You cannot specify both the remoteAppPath and the app"
            return None
        elif (options.remoteAppPath):
            options.app = options.remoteTestRoot + "/" + options.remoteAppPath
        elif (options.app == None):
            # Neither remoteAppPath nor app are set -- error
            print "ERROR: You must specify either appPath or app"
            return None

        if (options.xrePath == None):
            print "ERROR: You must specify the path to the controller xre directory"
            return None
        else:
            # Ensure xrepath is a full path
            options.xrePath = os.path.abspath(options.xrePath)

        # Default to <deviceroot>/reftest/reftest.log
        if (options.remoteLogFile == None):
            options.remoteLogFile = 'reftest.log'

        options.localLogName = options.remoteLogFile
        options.remoteLogFile = options.remoteTestRoot + '/' + options.remoteLogFile

        # Ensure that the options.logfile (which the base class uses) is set to
        # the remote setting when running remote. Also, if the user set the
        # log file name there, use that instead of reusing the remotelogfile as above.
        if (options.logFile):
            # If the user specified a local logfile name use that
            options.localLogName = options.logFile

        options.logFile = options.remoteLogFile

        if (options.pidFile != ""):
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # httpd-path is specified by standard makefile targets and may be specified
        # on the command line to select a particular version of httpd.js. If not
        # specified, try to select the one from hostutils.zip, as required in bug 882932.
        if not options.httpdPath:
            options.httpdPath = os.path.join(options.utilityPath, "components")

        # TODO: Copied from main, but I think these are no longer used in a post xulrunner world
        #options.xrePath = options.remoteTestRoot + self.automation._product + '/xulrunner'
        #options.utilityPath = options.testRoot + self.automation._product + '/bin'
        return options

class ReftestServer:
    """ Web server used to serve Reftests, for closer fidelity to the real web.
        It is virtually identical to the server used in mochitest and will only
        be used for running reftests remotely.
        Bug 581257 has been filed to refactor this wrapper around httpd.js into
        it's own class and use it in both remote and non-remote testing. """

    def __init__(self, automation, options, scriptDir):
        self.automation = automation
        self._utilityPath = options.utilityPath
        self._xrePath = options.xrePath
        self._profileDir = options.serverProfilePath
        self.webServer = options.remoteWebServer
        self.httpPort = options.httpPort
        self.scriptDir = scriptDir
        self.pidFile = options.pidFile
        self._httpdPath = os.path.abspath(options.httpdPath)
        self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % { "server" : self.webServer, "port" : self.httpPort }

    def start(self):
        "Run the Refest server, returning the process ID of the server."

        env = self.automation.environment(xrePath = self._xrePath)
        env["XPCOM_DEBUG_BREAK"] = "warn"
        if self.automation.IS_WIN32:
            env["PATH"] = env["PATH"] + ";" + self._xrePath

        args = ["-g", self._xrePath,
                "-v", "170",
                "-f", os.path.join(self._httpdPath, "httpd.js"),
                "-e", "const _PROFILE_PATH = '%(profile)s';const _SERVER_PORT = '%(port)s'; const _SERVER_ADDR ='%(server)s';" %
                       {"profile" : self._profileDir.replace('\\', '\\\\'), "port" : self.httpPort, "server" : self.webServer },
                "-f", os.path.join(self.scriptDir, "server.js")]

        xpcshell = os.path.join(self._utilityPath,
                                "xpcshell" + self.automation.BIN_SUFFIX)

        if not os.access(xpcshell, os.F_OK):
            raise Exception('xpcshell not found at %s' % xpcshell)
        if self.automation.elf_arm(xpcshell):
            raise Exception('xpcshell at %s is an ARM binary; please use '
                            'the --utility-path argument to specify the path '
                            'to a desktop version.' % xpcshell)

        self._process = self.automation.Process([xpcshell] + args, env = env)
        pid = self._process.pid
        if pid < 0:
            print "TEST-UNEXPECTED-FAIL | remotereftests.py | Error starting server."
            return 2
        self.automation.log.info("INFO | remotereftests.py | Server pid: %d", pid)

        if (self.pidFile != ""):
            f = open(self.pidFile + ".xpcshell.pid", 'w')
            f.write("%s" % pid)
            f.close()

    def ensureReady(self, timeout):
        assert timeout >= 0

        aliveFile = os.path.join(self._profileDir, "server_alive.txt")
        i = 0
        while i < timeout:
            if os.path.exists(aliveFile):
                break
            time.sleep(1)
            i += 1
        else:
            print "TEST-UNEXPECTED-FAIL | remotereftests.py | Timed out while waiting for server startup."
            self.stop()
            return 1

    def stop(self):
        if hasattr(self, '_process'):
            try:
                c = urllib2.urlopen(self.shutdownURL)
                c.read()
                c.close()

                rtncode = self._process.poll()
                if (rtncode == None):
                    self._process.terminate()
            except:
                self._process.kill()

class RemoteReftest(RefTest):
    remoteApp = ''

    def __init__(self, automation, devicemanager, options, scriptDir):
        RefTest.__init__(self)
        self.automation = automation
        self._devicemanager = devicemanager
        self.scriptDir = scriptDir
        self.remoteApp = options.app
        self.remoteProfile = options.remoteProfile
        self.remoteTestRoot = options.remoteTestRoot
        self.remoteLogFile = options.remoteLogFile
        self.localLogName = options.localLogName
        self.pidFile = options.pidFile
        if self.automation.IS_DEBUG_BUILD:
            self.SERVER_STARTUP_TIMEOUT = 180
        else:
            self.SERVER_STARTUP_TIMEOUT = 90
        self.automation.deleteANRs()
        self.automation.deleteTombstones()

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
        remoteUtilityPath = options.utilityPath
        localAutomation = Automation()
        localAutomation.IS_WIN32 = False
        localAutomation.IS_LINUX = False
        localAutomation.IS_MAC = False
        localAutomation.UNIXISH = False
        hostos = sys.platform
        if (hostos == 'mac' or  hostos == 'darwin'):
            localAutomation.IS_MAC = True
        elif (hostos == 'linux' or hostos == 'linux2'):
            localAutomation.IS_LINUX = True
            localAutomation.UNIXISH = True
        elif (hostos == 'win32' or hostos == 'win64'):
            localAutomation.BIN_SUFFIX = ".exe"
            localAutomation.IS_WIN32 = True

        paths = [options.xrePath, localAutomation.DIST_BIN, self.automation._product, os.path.join('..', self.automation._product)]
        options.xrePath = self.findPath(paths)
        if options.xrePath == None:
            print "ERROR: unable to find xulrunner path for %s, please specify with --xre-path" % (os.name)
            return 1
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
            return 1

        options.serverProfilePath = tempfile.mkdtemp()
        self.server = ReftestServer(localAutomation, options, self.scriptDir)
        retVal = self.server.start()
        if retVal:
            return retVal
        retVal = self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)
        if retVal:
            return retVal

        options.xrePath = remoteXrePath
        options.utilityPath = remoteUtilityPath
        return 0

    def stopWebServer(self, options):
        self.server.stop()

    def createReftestProfile(self, options, reftestlist):
        profile = RefTest.createReftestProfile(self, options, reftestlist, server=options.remoteWebServer)
        profileDir = profile.profile

        prefs = {}
        prefs["browser.firstrun.show.localepicker"] = False
        prefs["font.size.inflation.emPerLine"] = 0
        prefs["font.size.inflation.minTwips"] = 0
        prefs["reftest.remote"] = True
        # Set a future policy version to avoid the telemetry prompt.
        prefs["toolkit.telemetry.prompted"] = 999
        prefs["toolkit.telemetry.notifiedOptOut"] = 999
        prefs["reftest.uri"] = "%s" % reftestlist
        prefs["datareporting.policy.dataSubmissionPolicyBypassAcceptance"] = True

        # Point the url-classifier to the local testing server for fast failures
        prefs["browser.safebrowsing.gethashURL"] = "http://127.0.0.1:8888/safebrowsing-dummy/gethash"
        prefs["browser.safebrowsing.updateURL"] = "http://127.0.0.1:8888/safebrowsing-dummy/update"
        # Point update checks to the local testing server for fast failures
        prefs["extensions.update.url"] = "http://127.0.0.1:8888/extensions-dummy/updateURL"
        prefs["extensions.update.background.url"] = "http://127.0.0.1:8888/extensions-dummy/updateBackgroundURL"
        prefs["extensions.blocklist.url"] = "http://127.0.0.1:8888/extensions-dummy/blocklistURL"
        prefs["extensions.hotfix.url"] = "http://127.0.0.1:8888/extensions-dummy/hotfixURL"
        # Turn off extension updates so they don't bother tests
        prefs["extensions.update.enabled"] = False
        # Make sure opening about:addons won't hit the network
        prefs["extensions.webservice.discoverURL"] = "http://127.0.0.1:8888/extensions-dummy/discoveryURL"
        # Make sure AddonRepository won't hit the network
        prefs["extensions.getAddons.maxResults"] = 0
        prefs["extensions.getAddons.get.url"] = "http://127.0.0.1:8888/extensions-dummy/repositoryGetURL"
        prefs["extensions.getAddons.getWithPerformance.url"] = "http://127.0.0.1:8888/extensions-dummy/repositoryGetWithPerformanceURL"
        prefs["extensions.getAddons.search.browseURL"] = "http://127.0.0.1:8888/extensions-dummy/repositoryBrowseURL"
        prefs["extensions.getAddons.search.url"] = "http://127.0.0.1:8888/extensions-dummy/repositorySearchURL"
        # Make sure that opening the plugins check page won't hit the network
        prefs["plugins.update.url"] = "http://127.0.0.1:8888/plugins-dummy/updateCheckURL"
        prefs["layout.css.devPixelsPerPx"] = "1.0"

        # Disable skia-gl: see bug 907351
        prefs["gfx.canvas.azure.accelerated"] = False

        # Set the extra prefs.
        profile.set_preferences(prefs)

        try:
            self._devicemanager.pushDir(profileDir, options.remoteProfile)
        except devicemanager.DMError:
            print "Automation Error: Failed to copy profiledir to device"
            raise

        return profile

    def copyExtraFilesToProfile(self, options, profile):
        profileDir = profile.profile
        RefTest.copyExtraFilesToProfile(self, options, profile)
        try:
            self._devicemanager.pushDir(profileDir, options.remoteProfile)
        except devicemanager.DMError:
            print "Automation Error: Failed to copy extra files to device"
            raise

    def getManifestPath(self, path):
        return path

    def printDeviceInfo(self, printLogcat=False):
        try:
            if printLogcat:
                logcat = self._devicemanager.getLogcat(filterOutRegexps=fennecLogcatFilters)
                print ''.join(logcat)
            print "Device info: %s" % self._devicemanager.getInfo()
            print "Test root: %s" % self._devicemanager.deviceRoot
        except devicemanager.DMError:
            print "WARNING: Error getting device information"

    def environment(self, **kwargs):
     return self.automation.environment(**kwargs)

    def runApp(self, profile, binary, cmdargs, env,
               timeout=None, debuggerInfo=None,
               symbolsPath=None, options=None):
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

    def cleanup(self, profileDir):
        # Pull results back from device
        if self.remoteLogFile and \
                self._devicemanager.fileExists(self.remoteLogFile):
            self._devicemanager.getFile(self.remoteLogFile, self.localLogName)
        else:
            print "WARNING: Unable to retrieve log file (%s) from remote " \
                "device" % self.remoteLogFile
        self._devicemanager.removeDir(self.remoteProfile)
        self._devicemanager.removeDir(self.remoteTestRoot)
        RefTest.cleanup(self, profileDir)
        if (self.pidFile != ""):
            try:
                os.remove(self.pidFile)
                os.remove(self.pidFile + ".xpcshell.pid")
            except:
                print "Warning: cleaning up pidfile '%s' was unsuccessful from the test harness" % self.pidFile

def main(args):
    automation = RemoteAutomation(None)
    parser = RemoteOptions(automation)
    options, args = parser.parse_args()

    if (options.deviceIP == None):
        print "Error: you must provide a device IP to connect to via the --device option"
        return 1

    try:
        if (options.dm_trans == "adb"):
            if (options.deviceIP):
                dm = droid.DroidADB(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
            else:
                dm = droid.DroidADB(None, None, deviceRoot=options.remoteTestRoot)
        else:
            dm = droid.DroidSUT(options.deviceIP, options.devicePort, deviceRoot=options.remoteTestRoot)
    except devicemanager.DMError:
        print "Automation Error: exception while initializing devicemanager.  Most likely the device is not in a testable state."
        return 1

    automation.setDeviceManager(dm)

    if (options.remoteProductName != None):
        automation.setProduct(options.remoteProductName)

    # Set up the defaults and ensure options are set
    options = parser.verifyRemoteOptions(options)
    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        return 1

    if not options.ignoreWindowSize:
        parts = dm.getInfo('screen')['screen'][0].split()
        width = int(parts[0].split(':')[1])
        height = int(parts[1].split(':')[1])
        if (width < 1050 or height < 1050):
            print "ERROR: Invalid screen resolution %sx%s, please adjust to 1366x1050 or higher" % (width, height)
            return 1

    automation.setAppName(options.app)
    automation.setRemoteProfile(options.remoteProfile)
    automation.setRemoteLog(options.remoteLogFile)
    reftest = RemoteReftest(automation, dm, options, SCRIPT_DIRECTORY)
    options = parser.verifyCommonOptions(options, reftest)

    # Hack in a symbolic link for jsreftest
    os.system("ln -s ../jsreftest " + str(os.path.join(SCRIPT_DIRECTORY, "jsreftest")))

    # Dynamically build the reftest URL if possible, beware that args[0] should exist 'inside' the webroot
    manifest = args[0]
    if os.path.exists(os.path.join(SCRIPT_DIRECTORY, args[0])):
        manifest = "http://" + str(options.remoteWebServer) + ":" + str(options.httpPort) + "/" + args[0]
    elif os.path.exists(args[0]):
        manifestPath = os.path.abspath(args[0]).split(SCRIPT_DIRECTORY)[1].strip('/')
        manifest = "http://" + str(options.remoteWebServer) + ":" + str(options.httpPort) + "/" + manifestPath
    else:
        print "ERROR: Could not find test manifest '%s'" % manifest
        return 1

    # Start the webserver
    retVal = reftest.startWebServer(options)
    if retVal:
        return retVal

    procName = options.app.split('/')[-1]
    if (dm.processExist(procName)):
        dm.killProcess(procName)

    reftest.printDeviceInfo()

#an example manifest name to use on the cli
#    manifest = "http://" + options.remoteWebServer + "/reftests/layout/reftests/reftest-sanity/reftest.list"
    retVal = 0
    try:
        cmdlineArgs = ["-reftest", manifest]
        if options.bootstrap:
            cmdlineArgs = []
        dm.recordLogcat()
        retVal = reftest.runTests(manifest, options, cmdlineArgs)
    except:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        retVal = 1

    reftest.stopWebServer(options)

    reftest.printDeviceInfo(printLogcat=True)

    return retVal

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

