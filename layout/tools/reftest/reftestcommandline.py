import argparse
import os
from collections import OrderedDict
from urlparse import urlparse

here = os.path.abspath(os.path.dirname(__file__))


class ReftestArgumentsParser(argparse.ArgumentParser):
    def __init__(self, **kwargs):
        super(ReftestArgumentsParser, self).__init__(**kwargs)

        # Try to import a MozbuildObject. Success indicates that we are
        # running from a source tree. This allows some defaults to be set
        # from the source tree.
        try:
            from mozbuild.base import MozbuildObject
            self.build_obj = MozbuildObject.from_environment(cwd=here)
        except ImportError:
            self.build_obj = None

        self.add_argument("--xre-path",
                          action="store",
                          type=str,
                          dest="xrePath",
                          # individual scripts will set a sane default
                          default=None,
                          help="absolute path to directory containing XRE (probably xulrunner)")

        self.add_argument("--symbols-path",
                          action="store",
                          type=str,
                          dest="symbolsPath",
                          default=None,
                          help="absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")

        self.add_argument("--debugger",
                          action="store",
                          dest="debugger",
                          help="use the given debugger to launch the application")

        self.add_argument("--debugger-args",
                          action="store",
                          dest="debuggerArgs",
                          help="pass the given args to the debugger _before_ "
                          "the application on the command line")

        self.add_argument("--debugger-interactive",
                          action="store_true",
                          dest="debuggerInteractive",
                          help="prevents the test harness from redirecting "
                          "stdout and stderr for interactive debuggers")

        self.add_argument("--appname",
                          action="store",
                          type=str,
                          dest="app",
                          default=None,
                          help="absolute path to application, overriding default")

        self.add_argument("--extra-profile-file",
                          action="append",
                          dest="extraProfileFiles",
                          default=[],
                          help="copy specified files/dirs to testing profile")

        self.add_argument("--timeout",
                          action="store",
                          dest="timeout",
                          type=int,
                          default=5 * 60,  # 5 minutes per bug 479518
                          help="reftest will timeout in specified number of seconds. [default %(default)s].")

        self.add_argument("--leak-threshold",
                          action="store",
                          type=int,
                          dest="defaultLeakThreshold",
                          default=0,
                          help="fail if the number of bytes leaked in default "
                          "processes through refcounted objects (or bytes "
                          "in classes with MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) "
                          "is greater than the given number")

        self.add_argument("--utility-path",
                          action="store",
                          type=str,
                          dest="utilityPath",
                          default="bindir",
                          help="absolute path to directory containing utility "
                          "programs (xpcshell, ssltunnel, certutil)")

        self.add_argument("--total-chunks",
                          type=int,
                          dest="totalChunks",
                          help="how many chunks to split the tests up into")

        self.add_argument("--this-chunk",
                          type=int,
                          dest="thisChunk",
                          help="which chunk to run between 1 and --total-chunks")

        self.add_argument("--log-file",
                          action="store",
                          type=str,
                          dest="logFile",
                          default=None,
                          help="file to log output to in addition to stdout")

        self.add_argument("--skip-slow-tests",
                          dest="skipSlowTests",
                          action="store_true",
                          default=False,
                          help="skip tests marked as slow when running")

        self.add_argument("--ignore-window-size",
                          dest="ignoreWindowSize",
                          action="store_true",
                          default=False,
                          help="ignore the window size, which may cause spurious failures and passes")

        self.add_argument("--install-extension",
                          action="append",
                          dest="extensionsToInstall",
                          default=[],
                          help="install the specified extension in the testing profile. "
                          "The extension file's name should be <id>.xpi where <id> is "
                          "the extension's id as indicated in its install.rdf. "
                          "An optional path can be specified too.")

        self.add_argument("--setenv",
                          action="append",
                          type=str,
                          default=[],
                          dest="environment",
                          metavar="NAME=VALUE",
                          help="sets the given variable in the application's "
                          "environment")

        self.add_argument("--filter",
                          action="store",
                          type=str,
                          dest="filter",
                          help="specifies a regular expression (as could be passed to the JS "
                          "RegExp constructor) to test against URLs in the reftest manifest; "
                          "only test items that have a matching test URL will be run.")

        self.add_argument("--shuffle",
                          action="store_true",
                          default=False,
                          dest="shuffle",
                          help="run reftests in random order")

        self.add_argument("--focus-filter-mode",
                          action="store",
                          type=str,
                          dest="focusFilterMode",
                          default="all",
                          help="filters tests to run by whether they require focus. "
                          "Valid values are `all', `needs-focus', or `non-needs-focus'. "
                          "Defaults to `all'.")

        self.add_argument("--e10s",
                          action="store_true",
                          default=False,
                          dest="e10s",
                          help="enables content processes")

        self.add_argument("--setpref",
                          action="append",
                          type=str,
                          default=[],
                          dest="extraPrefs",
                          metavar="PREF=VALUE",
                          help="defines an extra user preference")

        self.add_argument("--reftest-extension-path",
                          action="store",
                          dest="reftestExtensionPath",
                          help="Path to the reftest extension")

        self.add_argument("--special-powers-extension-path",
                          action="store",
                          dest="specialPowersExtensionPath",
                          help="Path to the special powers extension")

        self.add_argument("--suite",
                          choices=["reftest", "crashtest", "jstestbrowser"],
                          default=None,
                          help=argparse.SUPPRESS)

        self.add_argument("tests",
                          metavar="TEST_PATH",
                          nargs="*",
                          help="Path to test file, manifest file, or directory containing tests")

    def get_ip(self):
        import moznetwork
        if os.name != "nt":
            return moznetwork.get_ip()
        else:
            self.error(
                "ERROR: you must specify a --remote-webserver=<ip address>\n")

    def set_default_suite(self, options):
        manifests = OrderedDict([("reftest.list", "reftest"),
                                 ("crashtests.list", "crashtest"),
                                 ("jstests.list", "jstestbrowser")])

        for test_path in options.tests:
            file_name = os.path.basename(test_path)
            if file_name in manifests:
                options.suite = manifests[file_name]
                return

        for test_path in options.tests:
            for manifest_file, suite in manifests.iteritems():
                if os.path.exists(os.path.join(test_path, manifest_file)):
                    options.suite = suite
                    return

        self.error("Failed to determine test suite; supply --suite to set this explicitly")

    def validate(self, options, reftest):
        import sys

        if not options.tests:
            # Can't just set this in the argument parser because mach will set a default
            self.error("Must supply at least one path to a manifest file, test directory, or test file to run.")

        if options.suite is None:
            self.set_default_suite(options)

        if options.totalChunks is not None and options.thisChunk is None:
            self.error(
                "thisChunk must be specified when totalChunks is specified")

        if options.totalChunks:
            if not 1 <= options.thisChunk <= options.totalChunks:
                self.error("thisChunk must be between 1 and totalChunks")

        if options.logFile:
            options.logFile = reftest.getFullPath(options.logFile)

        if options.xrePath is not None:
            if not os.access(options.xrePath, os.F_OK):
                self.error("--xre-path '%s' not found" % options.xrePath)
            if not os.path.isdir(options.xrePath):
                self.error("--xre-path '%s' is not a directory" %
                           options.xrePath)
            options.xrePath = reftest.getFullPath(options.xrePath)

        if options.reftestExtensionPath is None:
            if self.build_obj is not None:
                options.reftestExtensionPath = os.path.join(self.build_obj.topobjdir, "_tests",
                                                            "reftest", "reftest")
            else:
                options.reftestExtensionPath = os.path.join(here, "reftest")

        if (options.specialPowersExtensionPath is None and
            options.suite in ["crashtest", "jstestbrowser"]):
            if self.build_obj is not None:
                options.specialPowersExtensionPath = os.path.join(self.build_obj.topobjdir, "_tests",
                                                                  "reftest", "specialpowers")
            else:
                options.specialPowersExtensionPath = os.path.join(
                    here, "specialpowers")

        options.leakThresholds = {
            "default": options.defaultLeakThreshold,
            "tab": 5000,  # See dependencies of bug 1051230.
        }


class DesktopArgumentsParser(ReftestArgumentsParser):
    def __init__(self, **kwargs):
        super(DesktopArgumentsParser, self).__init__(**kwargs)

        self.add_argument("--run-tests-in-parallel",
                          action="store_true",
                          default=False,
                          dest="runTestsInParallel",
                          help="run tests in parallel if possible")

        self.add_argument("--ipc",
                          action="store_true",
                          default=False,
                          help="Run in out-of-processes mode")

    def _prefs_oop(self):
        import mozinfo
        prefs = ["layers.async-pan-zoom.enabled=true",
                 "browser.tabs.remote.autostart=true"]
        if mozinfo.os == "win":
            prefs.append("layers.acceleration.disabled=true")

        return prefs

    def _prefs_gpu(self):
        if mozinfo.os != "win":
            return ["layers.acceleration.force-enabled=true"]
        return []

    def validate(self, options, reftest):
        super(DesktopArgumentsParser, self).validate(options, reftest)

        if options.ipc:
            for item in self._prefs_oop():
                if item not in options.extraPrefs:
                    options.extraPrefs.append(item)

        if options.runTestsInParallel:
            if options.logFile is not None:
                self.error("cannot specify logfile with parallel tests")
            if options.totalChunks is not None or options.thisChunk is not None:
                self.error(
                    "cannot specify thisChunk or totalChunks with parallel tests")
            if options.focusFilterMode != "all":
                self.error("cannot specify focusFilterMode with parallel tests")
            if options.debugger is not None:
                self.error("cannot specify a debugger with parallel tests")

        if not options.tests:
            self.error("No test files specified.")

        if options.app is None:
            bin_dir = (self.build_obj.get_binary_path() if
                       self.build_obj and self.build_obj.substs[
                           'MOZ_BUILD_APP'] != 'mobile/android'
                       else None)

            if bin_dir:
                options.app = bin_dir
            else:
                self.error(
                    "could not find the application path, --appname must be specified")

        options.app = reftest.getFullPath(options.app)
        if not os.path.exists(options.app):
            self.error("""Error: Path %(app)s doesn't exist.
            Are you executing $objdir/_tests/reftest/runreftest.py?"""
                       % {"app": options.app})

        if options.xrePath is None:
            options.xrePath = os.path.dirname(options.app)

        if options.symbolsPath and len(urlparse(options.symbolsPath).scheme) < 2:
            options.symbolsPath = reftest.getFullPath(options.symbolsPath)

        options.utilityPath = reftest.getFullPath(options.utilityPath)


class B2GArgumentParser(ReftestArgumentsParser):
    def __init__(self, **kwargs):
        super(B2GArgumentParser, self).__init__(**kwargs)

        self.add_argument("--browser-arg",
                          action="store",
                          type=str,
                          dest="browser_arg",
                          help="Optional command-line arg to pass to the browser")

        self.add_argument("--b2gpath",
                          action="store",
                          type=str,
                          dest="b2gPath",
                          help="path to B2G repo or qemu dir")

        self.add_argument("--marionette",
                          action="store",
                          type=str,
                          dest="marionette",
                          help="host:port to use when connecting to Marionette")

        self.add_argument("--emulator",
                          action="store",
                          type=str,
                          dest="emulator",
                          help="Architecture of emulator to use: x86 or arm")

        self.add_argument("--emulator-res",
                          action="store",
                          type=str,
                          dest="emulator_res",
                          help="Emulator resolution of the format '<width>x<height>'")

        self.add_argument("--no-window",
                          action="store_true",
                          dest="noWindow",
                          default=False,
                          help="Pass --no-window to the emulator")

        self.add_argument("--adbpath",
                          action="store",
                          type=str,
                          dest="adb_path",
                          default="adb",
                          help="path to adb")

        self.add_argument("--deviceIP",
                          action="store",
                          type=str,
                          dest="deviceIP",
                          help="ip address of remote device to test")

        self.add_argument("--devicePort",
                          action="store",
                          type=str,
                          dest="devicePort",
                          default="20701",
                          help="port of remote device to test")

        self.add_argument("--remote-logfile",
                          action="store",
                          type=str,
                          dest="remoteLogFile",
                          help="Name of log file on the device relative to the device root.  PLEASE ONLY USE A FILENAME.")

        self.add_argument("--remote-webserver",
                          action="store",
                          type=str,
                          dest="remoteWebServer",
                          help="ip address where the remote web server is hosted at")

        self.add_argument("--http-port",
                          action="store",
                          type=str,
                          dest="httpPort",
                          help="ip address where the remote web server is hosted at")

        self.add_argument("--ssl-port",
                          action="store",
                          type=str,
                          dest="sslPort",
                          help="ip address where the remote web server is hosted at")

        self.add_argument("--pidfile",
                          action="store",
                          type=str,
                          dest="pidFile",
                          default="",
                          help="name of the pidfile to generate")

        self.add_argument("--gecko-path",
                          action="store",
                          type=str,
                          dest="geckoPath",
                          help="the path to a gecko distribution that should "
                          "be installed on the emulator prior to test")

        self.add_argument("--logdir",
                          action="store",
                          type=str,
                          dest="logdir",
                          help="directory to store log files")

        self.add_argument('--busybox',
                          action='store',
                          type=str,
                          dest='busybox',
                          help="Path to busybox binary to install on device")

        self.add_argument("--httpd-path",
                          action="store",
                          type=str,
                          dest="httpdPath",
                          help="path to the httpd.js file")

        self.add_argument("--profile",
                          action="store",
                          type=str,
                          dest="profile",
                          help="for desktop testing, the path to the "
                          "gaia profile to use")

        self.add_argument("--desktop",
                          action="store_true",
                          dest="desktop",
                          default=False,
                          help="Run the tests on a B2G desktop build")

        self.add_argument("--mulet",
                          action="store_true",
                          dest="mulet",
                          default=False,
                          help="Run the tests on a B2G desktop build")

        self.add_argument("--enable-oop",
                          action="store_true",
                          dest="oop",
                          default=False,
                          help="Run the tests out of process")

        self.set_defaults(remoteTestRoot=None,
                          logFile="reftest.log",
                          autorun=True,
                          closeWhenDone=True,
                          testPath="")

    def validate_remote(self, options, automation):
        if not options.app:
            options.app = automation.DEFAULT_APP

        if not options.remoteTestRoot:
            options.remoteTestRoot = automation._devicemanager.deviceRoot + \
                "/reftest"

        options.remoteProfile = options.remoteTestRoot + "/profile"

        productRoot = options.remoteTestRoot + "/" + automation._product
        if options.utilityPath is None:
            options.utilityPath = productRoot + "/bin"

        if not options.httpPort:
            options.httpPort = automation.DEFAULT_HTTP_PORT

        if not options.sslPort:
            options.sslPort = automation.DEFAULT_SSL_PORT

        if options.remoteWebServer is None:
            options.remoteWebServer = self.get_ip()

        options.webServer = options.remoteWebServer

        if options.geckoPath and not options.emulator:
            self.error(
                "You must specify --emulator if you specify --gecko-path")

        if options.logdir and not options.emulator:
            self.error("You must specify --emulator if you specify --logdir")

        if options.remoteLogFile is None:
            options.remoteLogFile = "reftest.log"

        options.localLogName = options.remoteLogFile
        options.remoteLogFile = options.remoteTestRoot + \
            '/' + options.remoteLogFile

        # Ensure that the options.logfile (which the base class uses) is set to
        # the remote setting when running remote. Also, if the user set the
        # log file name there, use that instead of reusing the remotelogfile as
        # above.
        if (options.logFile):
            # If the user specified a local logfile name use that
            options.localLogName = options.logFile
        options.logFile = options.remoteLogFile

        # Only reset the xrePath if it wasn't provided
        if options.xrePath is None:
            options.xrePath = options.utilityPath
        options.xrePath = os.path.abspath(options.xrePath)

        if options.pidFile != "":
            f = open(options.pidFile, 'w')
            f.write("%s" % os.getpid())
            f.close()

        # httpd-path is specified by standard makefile targets and may be specified
        # on the command line to select a particular version of httpd.js. If not
        # specified, try to select the one from from the xre bundle, as
        # required in bug 882932.
        if not options.httpdPath:
            options.httpdPath = os.path.join(options.xrePath, "components")

        return options


class RemoteArgumentsParser(ReftestArgumentsParser):
    def __init__(self, **kwargs):
        super(RemoteArgumentsParser, self).__init__()

        # app, xrePath and utilityPath variables are set in main function
        self.set_defaults(logFile="reftest.log",
                          app="",
                          xrePath="",
                          utilityPath="",
                          localLogName=None)

        self.add_argument("--remote-app-path",
                          action="store",
                          type=str,
                          dest="remoteAppPath",
                          help="Path to remote executable relative to device root using only forward slashes.  Either this or app must be specified, but not both.")

        self.add_argument("--deviceIP",
                          action="store",
                          type=str,
                          dest="deviceIP",
                          help="ip address of remote device to test")

        self.add_argument("--deviceSerial",
                          action="store",
                          type=str,
                          dest="deviceSerial",
                          help="adb serial number of remote device to test")

        self.add_argument("--devicePort",
                          action="store",
                          type=str,
                          default="20701",
                          dest="devicePort",
                          help="port of remote device to test")

        self.add_argument("--remote-product-name",
                          action="store",
                          type=str,
                          dest="remoteProductName",
                          default="fennec",
                          help="Name of product to test - either fennec or firefox, defaults to fennec")

        self.add_argument("--remote-webserver",
                          action="store",
                          type=str,
                          dest="remoteWebServer",
                          help="IP Address of the webserver hosting the reftest content")

        self.add_argument("--http-port",
                          action="store",
                          type=str,
                          dest="httpPort",
                          help="port of the web server for http traffic")

        self.add_argument("--ssl-port",
                          action="store",
                          type=str,
                          dest="sslPort",
                          help="Port for https traffic to the web server")

        self.add_argument("--remote-logfile",
                          action="store",
                          type=str,
                          dest="remoteLogFile",
                          default="reftest.log",
                          help="Name of log file on the device relative to device root.  PLEASE USE ONLY A FILENAME.")

        self.add_argument("--pidfile",
                          action="store",
                          type=str,
                          dest="pidFile",
                          default="",
                          help="name of the pidfile to generate")

        self.add_argument("--bootstrap",
                          action="store_true",
                          dest="bootstrap",
                          default=False,
                          help="test with a bootstrap addon required for native Fennec")

        self.add_argument("--dm_trans",
                          action="store",
                          type=str,
                          dest="dm_trans",
                          default="sut",
                          help="the transport to use to communicate with device: [adb|sut]; default=sut")

        self.add_argument("--remoteTestRoot",
                          action="store",
                          type=str,
                          dest="remoteTestRoot",
                          help="remote directory to use as test root (eg. /mnt/sdcard/tests or /data/local/tests)")

        self.add_argument("--httpd-path",
                          action="store",
                          type=str,
                          dest="httpdPath",
                          help="path to the httpd.js file")

    def validate_remote(self, options, automation):
        # Ensure our defaults are set properly for everything we can infer
        if not options.remoteTestRoot:
            options.remoteTestRoot = automation._devicemanager.deviceRoot + \
                '/reftest'
        options.remoteProfile = options.remoteTestRoot + "/profile"

        if options.remoteWebServer is None:
            options.remoteWebServer = self.get_ip()

        # Verify that our remotewebserver is set properly
        if options.remoteWebServer == '127.0.0.1':
            self.error("ERROR: Either you specified the loopback for the remote webserver or ",
                       "your local IP cannot be detected.  Please provide the local ip in --remote-webserver")

        if not options.httpPort:
            options.httpPort = automation.DEFAULT_HTTP_PORT

        if not options.sslPort:
            options.sslPort = automation.DEFAULT_SSL_PORT

        # One of remoteAppPath (relative path to application) or the app (executable) must be
        # set, but not both.  If both are set, we destroy the user's selection for app
        # so instead of silently destroying a user specificied setting, we
        # error.
        if options.remoteAppPath and options.app:
            self.error(
                "ERROR: You cannot specify both the remoteAppPath and the app")
        elif options.remoteAppPath:
            options.app = options.remoteTestRoot + "/" + options.remoteAppPath
        elif options.app is None:
            # Neither remoteAppPath nor app are set -- error
            self.error("ERROR: You must specify either appPath or app")

        if options.xrePath is None:
            self.error(
                "ERROR: You must specify the path to the controller xre directory")
        else:
            # Ensure xrepath is a full path
            options.xrePath = os.path.abspath(options.xrePath)

        options.localLogName = options.remoteLogFile
        options.remoteLogFile = options.remoteTestRoot + \
            '/' + options.remoteLogFile

        # Ensure that the options.logfile (which the base class uses) is set to
        # the remote setting when running remote. Also, if the user set the
        # log file name there, use that instead of reusing the remotelogfile as
        # above.
        if options.logFile:
            # If the user specified a local logfile name use that
            options.localLogName = options.logFile

        options.logFile = options.remoteLogFile

        if options.pidFile != "":
            with open(options.pidFile, 'w') as f:
                f.write(str(os.getpid()))

        # httpd-path is specified by standard makefile targets and may be specified
        # on the command line to select a particular version of httpd.js. If not
        # specified, try to select the one from hostutils.zip, as required in
        # bug 882932.
        if not options.httpdPath:
            options.httpdPath = os.path.join(options.utilityPath, "components")

        if not options.ignoreWindowSize:
            parts = automation._devicemanager.getInfo(
                'screen')['screen'][0].split()
            width = int(parts[0].split(':')[1])
            height = int(parts[1].split(':')[1])
            if (width < 1050 or height < 1050):
                self.error("ERROR: Invalid screen resolution %sx%s, please adjust to 1366x1050 or higher" % (
                    width, height))
