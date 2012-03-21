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
#  Joel Maher <joel.maher@gmail.com> (Original Developer)
#  Clint Talbert <cmtalbert@gmail.com>
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

import sys
import os
import time
import tempfile

# We need to know our current directory so that we can serve our test files from it.
SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))

from runreftest import RefTest
from runreftest import ReftestOptions
from automation import Automation
import devicemanager, devicemanagerADB, devicemanagerSUT
from remoteautomation import RemoteAutomation

class RemoteOptions(ReftestOptions):
    def __init__(self, automation):
        ReftestOptions.__init__(self, automation)

        defaults = {}
        defaults["logFile"] = "reftest.log"
        # app, xrePath and utilityPath variables are set in main function
        defaults["remoteTestRoot"] = None
        defaults["app"] = ""
        defaults["xrePath"] = ""
        defaults["utilityPath"] = ""

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
        defaults["remoteWebServer"] = automation.getLanIp() 

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

        self.add_option("--enable-privilege", action="store_true", dest = "enablePrivilege",
                    help = "add webserver and port to the user.js file for remote script access and universalXPConnect")
        defaults["enablePrivilege"] = False

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

        defaults["localLogName"] = None

        self.set_defaults(**defaults)

    def verifyRemoteOptions(self, options):
        # Ensure our defaults are set properly for everything we can infer
        options.remoteTestRoot = self._automation._devicemanager.getDeviceRoot() + '/reftest'
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

        # TODO: Copied from main, but I think these are no longer used in a post xulrunner world
        #options.xrePath = options.remoteTestRoot + self._automation._product + '/xulrunner'
        #options.utilityPath = options.testRoot + self._automation._product + '/bin'
        return options

class ReftestServer:
    """ Web server used to serve Reftests, for closer fidelity to the real web.
        It is virtually identical to the server used in mochitest and will only
        be used for running reftests remotely.
        Bug 581257 has been filed to refactor this wrapper around httpd.js into
        it's own class and use it in both remote and non-remote testing. """

    def __init__(self, automation, options, scriptDir):
        self._automation = automation
        self._utilityPath = options.utilityPath
        self._xrePath = options.xrePath
        self._profileDir = options.serverProfilePath
        self.webServer = options.remoteWebServer
        self.httpPort = options.httpPort
        self.scriptDir = scriptDir
        self.pidFile = options.pidFile
        self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % { "server" : self.webServer, "port" : self.httpPort }

    def start(self):
        "Run the Refest server, returning the process ID of the server."
          
        env = self._automation.environment(xrePath = self._xrePath)
        env["XPCOM_DEBUG_BREAK"] = "warn"
        if self._automation.IS_WIN32:
            env["PATH"] = env["PATH"] + ";" + self._xrePath

        args = ["-g", self._xrePath,
                "-v", "170",
                "-f", os.path.join(self.scriptDir, "reftest/components/httpd.js"),
                "-e", "const _PROFILE_PATH = '%(profile)s';const _SERVER_PORT = '%(port)s'; const _SERVER_ADDR ='%(server)s';" % 
                       {"profile" : self._profileDir.replace('\\', '\\\\'), "port" : self.httpPort, "server" : self.webServer },
                "-f", os.path.join(self.scriptDir, "server.js")]

        xpcshell = os.path.join(self._utilityPath,
                                "xpcshell" + self._automation.BIN_SUFFIX)
        self._process = self._automation.Process([xpcshell] + args, env = env)
        pid = self._process.pid
        if pid < 0:
            print "Error starting server."
            sys.exit(2)
        self._automation.log.info("INFO | remotereftests.py | Server pid: %d", pid)

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
            print "Timed out while waiting for server startup."
            self.stop()
            sys.exit(1)

    def stop(self):
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
        RefTest.__init__(self, automation)
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

        options.serverProfilePath = tempfile.mkdtemp()
        self.server = ReftestServer(localAutomation, options, self.scriptDir)
        self.server.start()

        self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)
        options.xrePath = remoteXrePath
        options.utilityPath = remoteUtilityPath
         
    def stopWebServer(self, options):
        self.server.stop()

    def createReftestProfile(self, options, profileDir, reftestlist):
        RefTest.createReftestProfile(self, options, profileDir, reftestlist, server=options.remoteWebServer)

        # Turn off the locale picker screen
        fhandle = open(os.path.join(profileDir, "user.js"), 'a')
        fhandle.write("""
user_pref("browser.firstrun.show.localepicker", false);
user_pref("font.size.inflation.emPerLine", 0);
user_pref("font.size.inflation.minTwips", 0);
user_pref("reftest.remote", true);
user_pref("toolkit.telemetry.prompted", true);
user_pref("reftest.uri", "%s");
""" % reftestlist)

        #workaround for jsreftests.
        if options.enablePrivilege:
            fhandle.write("""
user_pref("capability.principal.codebase.p2.granted", "UniversalXPConnect");
user_pref("capability.principal.codebase.p2.id", "http://%s:%s");
""" % (options.remoteWebServer, options.httpPort))

        # Close the file
        fhandle.close()

        if (self._devicemanager.pushDir(profileDir, options.remoteProfile) == None):
            raise devicemanager.FileError("Failed to copy profiledir to device")

    def copyExtraFilesToProfile(self, options, profileDir):
        RefTest.copyExtraFilesToProfile(self, options, profileDir)
        if (self._devicemanager.pushDir(profileDir, options.remoteProfile) == None):
            raise devicemanager.FileError("Failed to copy extra files to device") 

    def getManifestPath(self, path):
        return path

    def cleanup(self, profileDir):
        # Pull results back from device
        if (self.remoteLogFile):
            self._devicemanager.getFile(self.remoteLogFile, self.localLogName)
        self._devicemanager.removeDir(self.remoteProfile)
        self._devicemanager.removeDir(self.remoteTestRoot)
        RefTest.cleanup(self, profileDir)
        if (self.pidFile != ""):
            try:
                os.remove(self.pidFile)
                os.remove(self.pidFile + ".xpcshell.pid")
            except:
                print "Warning: cleaning up pidfile '%s' was unsuccessful from the test harness" % self.pidFile

def main():
    automation = RemoteAutomation(None)
    parser = RemoteOptions(automation)
    options, args = parser.parse_args()

    if (options.deviceIP == None):
        print "Error: you must provide a device IP to connect to via the --device option"
        sys.exit(1)

    if (options.dm_trans == "adb"):
        if (options.deviceIP):
            dm = devicemanagerADB.DeviceManagerADB(options.deviceIP, options.devicePort)
        else:
            dm = devicemanagerADB.DeviceManagerADB(None, None)
    else:
         dm = devicemanagerSUT.DeviceManagerSUT(options.deviceIP, options.devicePort)
    automation.setDeviceManager(dm)

    if (options.remoteProductName != None):
        automation.setProduct(options.remoteProductName)

    # Set up the defaults and ensure options are set
    options = parser.verifyRemoteOptions(options)
    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        sys.exit(1)

    if not options.ignoreWindowSize:
        parts = dm.getInfo('screen')['screen'][0].split()
        width = int(parts[0].split(':')[1])
        height = int(parts[1].split(':')[1])
        if (width < 1050 or height < 1050):
            print "ERROR: Invalid screen resolution %sx%s, please adjust to 1366x1050 or higher" % (width, height)
            sys.exit(1)

    automation.setAppName(options.app)
    automation.setRemoteProfile(options.remoteProfile)
    automation.setRemoteLog(options.remoteLogFile)
    reftest = RemoteReftest(automation, dm, options, SCRIPT_DIRECTORY)

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
        sys.exit(1)

    # Start the webserver
    reftest.startWebServer(options)

    procName = options.app.split('/')[-1]
    if (dm.processExist(procName)):
        dm.killProcess(procName)

#an example manifest name to use on the cli
#    manifest = "http://" + options.remoteWebServer + "/reftests/layout/reftests/reftest-sanity/reftest.list"
    try:
        cmdlineArgs = ["-reftest", manifest]
        if options.bootstrap:
            cmdlineArgs = []
        reftest.runTests(manifest, options, cmdlineArgs)
    except:
        print "TEST-UNEXPECTED-FAIL | | exception while running reftests"
        reftest.stopWebServer(options)
        sys.exit(1)

    reftest.stopWebServer(options)

if __name__ == "__main__":
    main()
    
