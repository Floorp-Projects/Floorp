# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the reftest test harness.
"""

from optparse import OptionParser
from urlparse import urlparse
import collections
import multiprocessing
import os
import re
import shutil
import signal
import subprocess
import sys
import threading

SCRIPT_DIRECTORY = os.path.abspath(
    os.path.realpath(os.path.dirname(sys.argv[0])))
sys.path.insert(0, SCRIPT_DIRECTORY)

import mozcrash
import mozdebug
import mozinfo
import mozleak
import mozprocess
import mozprofile
import mozrunner
from mozrunner.utils import get_stack_fixer_function, test_environment
from mozscreenshot import printstatus, dump_screen

here = os.path.abspath(os.path.dirname(__file__))

try:
    from mozbuild.base import MozbuildObject
    build_obj = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build_obj = None

# set up logging handler a la automation.py.in for compatability
import logging
log = logging.getLogger()


def resetGlobalLog():
    while log.handlers:
        log.removeHandler(log.handlers[0])
    handler = logging.StreamHandler(sys.stdout)
    log.setLevel(logging.INFO)
    log.addHandler(handler)
resetGlobalLog()


def categoriesToRegex(categoryList):
    return "\\(" + ', '.join(["(?P<%s>\\d+) %s" % c for c in categoryList]) + "\\)"
summaryLines = [('Successful', [('pass', 'pass'), ('loadOnly', 'load only')]),
                ('Unexpected', [('fail', 'unexpected fail'),
                                ('pass', 'unexpected pass'),
                                ('asserts', 'unexpected asserts'),
                                ('fixedAsserts', 'unexpected fixed asserts'),
                                ('failedLoad', 'failed load'),
                                ('exception', 'exception')]),
                ('Known problems', [('knownFail', 'known fail'),
                                    ('knownAsserts', 'known asserts'),
                                    ('random', 'random'),
                                    ('skipped', 'skipped'),
                                    ('slow', 'slow')])]

# Python's print is not threadsafe.
printLock = threading.Lock()


class ReftestThread(threading.Thread):
    def __init__(self, cmdlineArgs):
        threading.Thread.__init__(self)
        self.cmdlineArgs = cmdlineArgs
        self.summaryMatches = {}
        self.retcode = -1
        for text, _ in summaryLines:
            self.summaryMatches[text] = None

    def run(self):
        with printLock:
            print "Starting thread with", self.cmdlineArgs
            sys.stdout.flush()
        process = subprocess.Popen(self.cmdlineArgs, stdout=subprocess.PIPE)
        for chunk in self.chunkForMergedOutput(process.stdout):
            with printLock:
                print chunk,
                sys.stdout.flush()
        self.retcode = process.wait()

    def chunkForMergedOutput(self, logsource):
        """Gather lines together that should be printed as one atomic unit.
        Individual test results--anything between 'REFTEST TEST-START' and
        'REFTEST TEST-END' lines--are an atomic unit.  Lines with data from
        summaries are parsed and the data stored for later aggregation.
        Other lines are considered their own atomic units and are permitted
        to intermix freely."""
        testStartRegex = re.compile("^REFTEST TEST-START")
        testEndRegex = re.compile("^REFTEST TEST-END")
        summaryHeadRegex = re.compile("^REFTEST INFO \\| Result summary:")
        summaryRegexFormatString = "^REFTEST INFO \\| (?P<message>{text}): (?P<total>\\d+) {regex}"
        summaryRegexStrings = [summaryRegexFormatString.format(text=text,
                                                               regex=categoriesToRegex(categories))
                               for (text, categories) in summaryLines]
        summaryRegexes = [re.compile(regex) for regex in summaryRegexStrings]

        for line in logsource:
            if testStartRegex.search(line) is not None:
                chunkedLines = [line]
                for lineToBeChunked in logsource:
                    chunkedLines.append(lineToBeChunked)
                    if testEndRegex.search(lineToBeChunked) is not None:
                        break
                yield ''.join(chunkedLines)
                continue

            haveSuppressedSummaryLine = False
            for regex in summaryRegexes:
                match = regex.search(line)
                if match is not None:
                    self.summaryMatches[match.group('message')] = match
                    haveSuppressedSummaryLine = True
                    break
            if haveSuppressedSummaryLine:
                continue

            if summaryHeadRegex.search(line) is None:
                yield line


class RefTest(object):
    oldcwd = os.getcwd()

    def __init__(self):
        self.update_mozinfo()
        self.lastTestSeen = 'reftest'
        self.haveDumpedScreen = False

    def update_mozinfo(self):
        """walk up directories to find mozinfo.json update the info"""
        # TODO: This should go in a more generic place, e.g. mozinfo

        path = SCRIPT_DIRECTORY
        dirs = set()
        while path != os.path.expanduser('~'):
            if path in dirs:
                break
            dirs.add(path)
            path = os.path.split(path)[0]
        mozinfo.find_and_update_from_json(*dirs)

    def getFullPath(self, path):
        "Get an absolute path relative to self.oldcwd."
        return os.path.normpath(os.path.join(self.oldcwd, os.path.expanduser(path)))

    def getManifestPath(self, path):
        "Get the path of the manifest, and for remote testing this function is subclassed to point to remote manifest"
        path = self.getFullPath(path)
        if os.path.isdir(path):
            defaultManifestPath = os.path.join(path, 'reftest.list')
            if os.path.exists(defaultManifestPath):
                path = defaultManifestPath
            else:
                defaultManifestPath = os.path.join(path, 'crashtests.list')
                if os.path.exists(defaultManifestPath):
                    path = defaultManifestPath
        return path

    def makeJSString(self, s):
        return '"%s"' % re.sub(r'([\\"])', r'\\\1', s)

    def createReftestProfile(self, options, manifest, server='localhost', port=0,
                             special_powers=True, profile_to_clone=None):
        """
          Sets up a profile for reftest.
          'manifest' is the path to the reftest.list file we want to test with.  This is used in
          the remote subclass in remotereftest.py so we can write it to a preference for the
          bootstrap extension.
        """

        locations = mozprofile.permissions.ServerLocations()
        locations.add_host(server, scheme='http', port=port)
        locations.add_host(server, scheme='https', port=port)

        # Set preferences for communication between our command line arguments
        # and the reftest harness.  Preferences that are required for reftest
        # to work should instead be set in reftest-cmdline.js .
        prefs = {}
        prefs['reftest.timeout'] = options.timeout * 1000
        if options.totalChunks:
            prefs['reftest.totalChunks'] = options.totalChunks
        if options.thisChunk:
            prefs['reftest.thisChunk'] = options.thisChunk
        if options.logFile:
            prefs['reftest.logFile'] = options.logFile
        if options.ignoreWindowSize:
            prefs['reftest.ignoreWindowSize'] = True
        if options.filter:
            prefs['reftest.filter'] = options.filter
        if options.shuffle:
            prefs['reftest.shuffle'] = True
        prefs['reftest.focusFilterMode'] = options.focusFilterMode

        # Ensure that telemetry is disabled, so we don't connect to the telemetry
        # server in the middle of the tests.
        prefs['toolkit.telemetry.enabled'] = False
        prefs['toolkit.telemetry.unified'] = False
        # Likewise for safebrowsing.
        prefs['browser.safebrowsing.enabled'] = False
        prefs['browser.safebrowsing.malware.enabled'] = False
        # Likewise for tracking protection.
        prefs['privacy.trackingprotection.enabled'] = False
        prefs['privacy.trackingprotection.pbmode.enabled'] = False
        # And for snippets.
        prefs['browser.snippets.enabled'] = False
        prefs['browser.snippets.syncPromo.enabled'] = False
        prefs['browser.snippets.firstrunHomepage.enabled'] = False
        # And for useragent updates.
        prefs['general.useragent.updates.enabled'] = False
        # And for webapp updates.  Yes, it is supposed to be an integer.
        prefs['browser.webapps.checkForUpdates'] = 0
        # And for about:newtab content fetch and pings.
        prefs['browser.newtabpage.directory.source'] = 'data:application/json,{"reftest":1}'
        prefs['browser.newtabpage.directory.ping'] = ''
        # Only allow add-ons from the profile and app and allow foreign
        # injection
        prefs["extensions.enabledScopes"] = 5
        prefs["extensions.autoDisableScopes"] = 0
        # Allow unsigned add-ons
        prefs['xpinstall.signatures.required'] = False

        # Don't use auto-enabled e10s
        prefs['browser.tabs.remote.autostart.1'] = False
        prefs['browser.tabs.remote.autostart.2'] = False
        if options.e10s:
            prefs['browser.tabs.remote.autostart'] = True

        for v in options.extraPrefs:
            thispref = v.split('=')
            if len(thispref) < 2:
                print "Error: syntax error in --setpref=" + v
                sys.exit(1)
            prefs[thispref[0]] = mozprofile.Preferences.cast(
                thispref[1].strip())

        # install the reftest extension bits into the profile
        addons = []
        addons.append(os.path.join(SCRIPT_DIRECTORY, "reftest"))

        # I would prefer to use "--install-extension reftest/specialpowers", but that requires tight coordination with
        # release engineering and landing on multiple branches at once.
        if special_powers and (manifest.endswith('crashtests.list') or manifest.endswith('jstests.list')):
            addons.append(os.path.join(SCRIPT_DIRECTORY, 'specialpowers'))
            # SpecialPowers requires insecure automation-only features that we
            # put behind a pref.
            prefs['security.turn_off_all_security_so_that_viruses_can_take_over_this_computer'] = True

        # Install distributed extensions, if application has any.
        distExtDir = os.path.join(options.app[:options.app.rfind(os.sep)],
                                  "distribution", "extensions")
        if os.path.isdir(distExtDir):
            for f in os.listdir(distExtDir):
                addons.append(os.path.join(distExtDir, f))

        # Install custom extensions.
        for f in options.extensionsToInstall:
            addons.append(self.getFullPath(f))

        kwargs = {'addons': addons,
                  'preferences': prefs,
                  'locations': locations}
        if profile_to_clone:
            profile = mozprofile.Profile.clone(profile_to_clone, **kwargs)
        else:
            profile = mozprofile.Profile(**kwargs)

        self.copyExtraFilesToProfile(options, profile)
        return profile

    def environment(self, **kwargs):
        kwargs['log'] = log
        return test_environment(**kwargs)

    def buildBrowserEnv(self, options, profileDir):
        browserEnv = self.environment(
            xrePath=options.xrePath, debugger=options.debugger)
        browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

        for v in options.environment:
            ix = v.find("=")
            if ix <= 0:
                print "Error: syntax error in --setenv=" + v
                return None
            browserEnv[v[:ix]] = v[ix + 1:]

        # Enable leaks detection to its own log file.
        self.leakLogFile = os.path.join(profileDir, "runreftest_leaks.log")
        browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leakLogFile
        return browserEnv

    def killNamedOrphans(self, pname):
        """ Kill orphan processes matching the given command name """
        log.info("Checking for orphan %s processes..." % pname)

        def _psInfo(line):
            if pname in line:
                log.info(line)
        process = mozprocess.ProcessHandler(['ps', '-f'],
                                            processOutputLine=_psInfo)
        process.run()
        process.wait()

        def _psKill(line):
            parts = line.split()
            if len(parts) == 3 and parts[0].isdigit():
                pid = int(parts[0])
                if parts[2] == pname and parts[1] == '1':
                    log.info("killing %s orphan with pid %d" % (pname, pid))
                    try:
                        os.kill(
                            pid, getattr(signal, "SIGKILL", signal.SIGTERM))
                    except Exception as e:
                        log.info("Failed to kill process %d: %s" %
                                 (pid, str(e)))
        process = mozprocess.ProcessHandler(['ps', '-o', 'pid,ppid,comm'],
                                            processOutputLine=_psKill)
        process.run()
        process.wait()

    def cleanup(self, profileDir):
        if profileDir:
            shutil.rmtree(profileDir, True)

    def runTests(self, testPath, options, cmdlineArgs=None):
        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing orphan servers before
        # trying to start new ones.
        self.killNamedOrphans('ssltunnel')
        self.killNamedOrphans('xpcshell')

        if not options.runTestsInParallel:
            return self.runSerialTests(testPath, options, cmdlineArgs)

        cpuCount = multiprocessing.cpu_count()

        # We have the directive, technology, and machine to run multiple test instances.
        # Experimentation says that reftests are not overly CPU-intensive, so we can run
        # multiple jobs per CPU core.
        #
        # Our Windows machines in automation seem to get upset when we run a lot of
        # simultaneous tests on them, so tone things down there.
        if sys.platform == 'win32':
            jobsWithoutFocus = cpuCount
        else:
            jobsWithoutFocus = 2 * cpuCount

        totalJobs = jobsWithoutFocus + 1
        perProcessArgs = [sys.argv[:] for i in range(0, totalJobs)]

        # First job is only needs-focus tests.  Remaining jobs are
        # non-needs-focus and chunked.
        perProcessArgs[0].insert(-1, "--focus-filter-mode=needs-focus")
        for (chunkNumber, jobArgs) in enumerate(perProcessArgs[1:], start=1):
            jobArgs[-1:-1] = ["--focus-filter-mode=non-needs-focus",
                              "--total-chunks=%d" % jobsWithoutFocus,
                              "--this-chunk=%d" % chunkNumber]

        for jobArgs in perProcessArgs:
            try:
                jobArgs.remove("--run-tests-in-parallel")
            except:
                pass
            jobArgs.insert(-1, "--no-run-tests-in-parallel")
            jobArgs[0:0] = [sys.executable, "-u"]

        threads = [ReftestThread(args) for args in perProcessArgs[1:]]
        for t in threads:
            t.start()

        while True:
            # The test harness in each individual thread will be doing timeout
            # handling on its own, so we shouldn't need to worry about any of
            # the threads hanging for arbitrarily long.
            for t in threads:
                t.join(10)
            if not any(t.is_alive() for t in threads):
                break

        # Run the needs-focus tests serially after the other ones, so we don't
        # have to worry about races between the needs-focus tests *actually*
        # needing focus and the dummy windows in the non-needs-focus tests
        # trying to focus themselves.
        focusThread = ReftestThread(perProcessArgs[0])
        focusThread.start()
        focusThread.join()

        # Output the summaries that the ReftestThread filters suppressed.
        summaryObjects = [collections.defaultdict(int) for s in summaryLines]
        for t in threads:
            for (summaryObj, (text, categories)) in zip(summaryObjects, summaryLines):
                threadMatches = t.summaryMatches[text]
                for (attribute, description) in categories:
                    amount = int(
                        threadMatches.group(attribute) if threadMatches else 0)
                    summaryObj[attribute] += amount
                amount = int(
                    threadMatches.group('total') if threadMatches else 0)
                summaryObj['total'] += amount

        print 'REFTEST INFO | Result summary:'
        for (summaryObj, (text, categories)) in zip(summaryObjects, summaryLines):
            details = ', '.join(["%d %s" % (summaryObj[attribute], description) for (
                attribute, description) in categories])
            print 'REFTEST INFO | ' + text + ': ' + str(summaryObj['total']) + ' (' + details + ')'

        return int(any(t.retcode != 0 for t in threads))

    def handleTimeout(self, timeout, proc, utilityPath, debuggerInfo):
        """handle process output timeout"""
        # TODO: bug 913975 : _processOutput should call self.processOutputLine
        # one more time one timeout (I think)
        log.error("TEST-UNEXPECTED-FAIL | %s | application timed out after %d seconds with no output" %
                  (self.lastTestSeen, int(timeout)))
        self.killAndGetStack(
            proc, utilityPath, debuggerInfo, dump_screen=not debuggerInfo)

    def dumpScreen(self, utilityPath):
        if self.haveDumpedScreen:
            log.info("Not taking screenshot here: see the one that was previously logged")
            return
        self.haveDumpedScreen = True
        dump_screen(utilityPath, log)

    def killAndGetStack(self, process, utilityPath, debuggerInfo, dump_screen=False):
        """
        Kill the process, preferrably in a way that gets us a stack trace.
        Also attempts to obtain a screenshot before killing the process
        if specified.
        """

        if dump_screen:
            self.dumpScreen(utilityPath)

        if mozinfo.info.get('crashreporter', True) and not debuggerInfo:
            if mozinfo.isWin:
                # We should have a "crashinject" program in our utility path
                crashinject = os.path.normpath(
                    os.path.join(utilityPath, "crashinject.exe"))
                if os.path.exists(crashinject):
                    status = subprocess.Popen(
                        [crashinject, str(process.pid)]).wait()
                    printstatus("crashinject", status)
                    if status == 0:
                        return
            else:
                try:
                    process.kill(sig=signal.SIGABRT)
                except OSError:
                    # https://bugzilla.mozilla.org/show_bug.cgi?id=921509
                    log.info("Can't trigger Breakpad, process no longer exists")
                return
        log.info("Can't trigger Breakpad, just killing process")
        process.kill()

    # output processing

    class OutputHandler(object):

        """line output handler for mozrunner"""

        def __init__(self, harness, utilityPath, symbolsPath=None, dump_screen_on_timeout=True):
            """
            harness -- harness instance
            dump_screen_on_timeout -- whether to dump the screen on timeout
            """
            self.harness = harness
            self.utilityPath = utilityPath
            self.symbolsPath = symbolsPath
            self.dump_screen_on_timeout = dump_screen_on_timeout
            self.stack_fixer_function = self.stack_fixer()

        def processOutputLine(self, line):
            """per line handler of output for mozprocess"""
            for handler in self.output_handlers():
                line = handler(line)
        __call__ = processOutputLine

        def output_handlers(self):
            """returns ordered list of output handlers"""
            return [self.fix_stack,
                    self.format,
                    self.record_last_test,
                    self.handle_timeout_and_dump_screen,
                    self.log,
                    ]

        def stack_fixer(self):
            """
            return get_stack_fixer_function, if any, to use on the output lines
            """
            return get_stack_fixer_function(self.utilityPath, self.symbolsPath)

        # output line handlers:
        # these take a line and return a line
        def fix_stack(self, line):
            if self.stack_fixer_function:
                return self.stack_fixer_function(line)
            return line

        def format(self, line):
            """format the line"""
            return line.rstrip().decode("UTF-8", "ignore")

        def record_last_test(self, line):
            """record last test on harness"""
            if "TEST-START" in line and "|" in line:
                self.harness.lastTestSeen = line.split("|")[1].strip()
            return line

        def handle_timeout_and_dump_screen(self, line):
            if self.dump_screen_on_timeout and "TEST-UNEXPECTED-FAIL" in line and "Test timed out" in line:
                self.harness.dumpScreen(self.utilityPath)
            return line

        def log(self, line):
            log.info(line)
            return line

    def runApp(self, profile, binary, cmdargs, env,
               timeout=None, debuggerInfo=None,
               symbolsPath=None, options=None):

        def timeoutHandler():
            self.handleTimeout(
                timeout, proc, options.utilityPath, debuggerInfo)

        interactive = False
        debug_args = None
        if debuggerInfo:
            interactive = debuggerInfo.interactive
            debug_args = [debuggerInfo.path] + debuggerInfo.args

        outputHandler = self.OutputHandler(harness=self,
                                           utilityPath=options.utilityPath,
                                           symbolsPath=symbolsPath,
                                           dump_screen_on_timeout=not debuggerInfo)

        kp_kwargs = {
            'kill_on_timeout': False,
            'cwd': SCRIPT_DIRECTORY,
            'onTimeout': [timeoutHandler],
            'processOutputLine': [outputHandler],
        }

        if interactive:
            # If an interactive debugger is attached,
            # don't use timeouts, and don't capture ctrl-c.
            timeout = None
            signal.signal(signal.SIGINT, lambda sigid, frame: None)

        if mozinfo.info.get('appname') == 'b2g' and mozinfo.info.get('toolkit') != 'gonk':
            runner_cls = mozrunner.Runner
        else:
            runner_cls = mozrunner.runners.get(mozinfo.info.get('appname', 'firefox'),
                                               mozrunner.Runner)
        runner = runner_cls(profile=profile,
                            binary=binary,
                            process_class=mozprocess.ProcessHandlerMixin,
                            cmdargs=cmdargs,
                            env=env,
                            process_args=kp_kwargs)
        runner.start(debug_args=debug_args,
                     interactive=interactive,
                     outputTimeout=timeout)
        proc = runner.process_handler
        status = runner.wait()
        runner.process_handler = None
        if timeout is None:
            didTimeout = False
        else:
            didTimeout = proc.didTimeout

        if status:
            log.info("TEST-UNEXPECTED-FAIL | %s | application terminated with exit code %s",
                     self.lastTestSeen, status)
        else:
            self.lastTestSeen = 'Main app process exited normally'

        crashed = mozcrash.check_for_crashes(os.path.join(profile.profile, "minidumps"),
                                             symbolsPath, test_name=self.lastTestSeen)
        runner.cleanup()
        if not status and crashed:
            status = 1
        return status

    def runSerialTests(self, testPath, options, cmdlineArgs=None):
        debuggerInfo = None
        if options.debugger:
            debuggerInfo = mozdebug.get_debugger_info(options.debugger, options.debuggerArgs,
                                                      options.debuggerInteractive)

        profileDir = None
        try:
            reftestlist = self.getManifestPath(testPath)
            if cmdlineArgs == None:
                cmdlineArgs = ['-reftest', reftestlist]
            profile = self.createReftestProfile(options, reftestlist)
            profileDir = profile.profile  # name makes more sense

            # browser environment
            browserEnv = self.buildBrowserEnv(options, profileDir)

            log.info("REFTEST INFO | runreftest.py | Running tests: start.\n")
            status = self.runApp(profile,
                                 binary=options.app,
                                 cmdargs=cmdlineArgs,
                                 # give the JS harness 30 seconds to deal with
                                 # its own timeouts
                                 env=browserEnv,
                                 timeout=options.timeout + 30.0,
                                 symbolsPath=options.symbolsPath,
                                 options=options,
                                 debuggerInfo=debuggerInfo)
            mozleak.process_leak_log(self.leakLogFile,
                                     leak_thresholds=options.leakThresholds,
                                     log=log,
                                     stack_fixer=get_stack_fixer_function(options.utilityPath,
                                                                          options.symbolsPath),
            )
            log.info("\nREFTEST INFO | runreftest.py | Running tests: end.")
        finally:
            self.cleanup(profileDir)
        return status

    def copyExtraFilesToProfile(self, options, profile):
        "Copy extra files or dirs specified on the command line to the testing profile."
        profileDir = profile.profile
        for f in options.extraProfileFiles:
            abspath = self.getFullPath(f)
            if os.path.isfile(abspath):
                if os.path.basename(abspath) == 'user.js':
                    extra_prefs = mozprofile.Preferences.read_prefs(abspath)
                    profile.set_preferences(extra_prefs)
                else:
                    shutil.copy2(abspath, profileDir)
            elif os.path.isdir(abspath):
                dest = os.path.join(profileDir, os.path.basename(abspath))
                shutil.copytree(abspath, dest)
            else:
                log.warning(
                    "WARNING | runreftest.py | Failed to copy %s to profile", abspath)
                continue


class ReftestOptions(OptionParser):

    def __init__(self):
        OptionParser.__init__(self)
        defaults = {}
        self.add_option("--xre-path",
                        action="store", type="string", dest="xrePath",
                        # individual scripts will set a sane default
                        default=None,
                        help="absolute path to directory containing XRE (probably xulrunner)")
        self.add_option("--symbols-path",
                        action="store", type="string", dest="symbolsPath",
                        default=None,
                        help="absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")
        self.add_option("--debugger",
                        action="store", dest="debugger",
                        help="use the given debugger to launch the application")
        self.add_option("--debugger-args",
                        action="store", dest="debuggerArgs",
                        help="pass the given args to the debugger _before_ "
                        "the application on the command line")
        self.add_option("--debugger-interactive",
                        action="store_true", dest="debuggerInteractive",
                        help="prevents the test harness from redirecting "
                        "stdout and stderr for interactive debuggers")
        self.add_option("--appname",
                        action="store", type="string", dest="app",
                        help="absolute path to application, overriding default")
        # Certain paths do not make sense when we're cross compiling Fennec.  This
        # logic is cribbed from the example in
        # python/mozbuild/mozbuild/mach_commands.py.
        defaults['app'] = build_obj.get_binary_path() if \
            build_obj and build_obj.substs['MOZ_BUILD_APP'] != 'mobile/android' else None

        self.add_option("--extra-profile-file",
                        action="append", dest="extraProfileFiles",
                        default=[],
                        help="copy specified files/dirs to testing profile")
        self.add_option("--timeout",
                        action="store", dest="timeout", type="int",
                        default=5 * 60,  # 5 minutes per bug 479518
                        help="reftest will timeout in specified number of seconds. [default %default s].")
        self.add_option("--leak-threshold",
                        action="store", type="int", dest="defaultLeakThreshold",
                        default=0,
                        help="fail if the number of bytes leaked in default "
                        "processes through refcounted objects (or bytes "
                               "in classes with MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) "
                               "is greater than the given number")
        self.add_option("--utility-path",
                        action="store", type="string", dest="utilityPath",
                        help="absolute path to directory containing utility "
                        "programs (xpcshell, ssltunnel, certutil)")
        defaults["utilityPath"] = build_obj.bindir if \
            build_obj and build_obj.substs['MOZ_BUILD_APP'] != 'mobile/android' else None

        self.add_option("--total-chunks",
                        type="int", dest="totalChunks",
                        help="how many chunks to split the tests up into")
        defaults["totalChunks"] = None

        self.add_option("--this-chunk",
                        type="int", dest="thisChunk",
                        help="which chunk to run between 1 and --total-chunks")
        defaults["thisChunk"] = None

        self.add_option("--log-file",
                        action="store", type="string", dest="logFile",
                        default=None,
                        help="file to log output to in addition to stdout")
        defaults["logFile"] = None

        self.add_option("--skip-slow-tests",
                        dest="skipSlowTests", action="store_true",
                        help="skip tests marked as slow when running")
        defaults["skipSlowTests"] = False

        self.add_option("--ignore-window-size",
                        dest="ignoreWindowSize", action="store_true",
                        help="ignore the window size, which may cause spurious failures and passes")
        defaults["ignoreWindowSize"] = False

        self.add_option("--install-extension",
                        action="append", dest="extensionsToInstall",
                        help="install the specified extension in the testing profile. "
                        "The extension file's name should be <id>.xpi where <id> is "
                               "the extension's id as indicated in its install.rdf. "
                               "An optional path can be specified too.")
        defaults["extensionsToInstall"] = []

        self.add_option("--run-tests-in-parallel",
                        action="store_true", dest="runTestsInParallel",
                        help="run tests in parallel if possible")
        self.add_option("--no-run-tests-in-parallel",
                        action="store_false", dest="runTestsInParallel",
                        help="do not run tests in parallel")
        defaults["runTestsInParallel"] = False

        self.add_option("--setenv",
                        action="append", type="string",
                        dest="environment", metavar="NAME=VALUE",
                        help="sets the given variable in the application's "
                        "environment")
        defaults["environment"] = []

        self.add_option("--filter",
                        action="store", type="string", dest="filter",
                        help="specifies a regular expression (as could be passed to the JS "
                        "RegExp constructor) to test against URLs in the reftest manifest; "
                               "only test items that have a matching test URL will be run.")
        defaults["filter"] = None

        self.add_option("--shuffle",
                        action="store_true", dest="shuffle",
                        help="run reftests in random order")
        defaults["shuffle"] = False

        self.add_option("--focus-filter-mode",
                        action="store", type="string", dest="focusFilterMode",
                        help="filters tests to run by whether they require focus. "
                        "Valid values are `all', `needs-focus', or `non-needs-focus'. "
                               "Defaults to `all'.")
        defaults["focusFilterMode"] = "all"

        self.add_option("--e10s",
                        action="store_true",
                        dest="e10s",
                        help="enables content processes")
        defaults["e10s"] = False

        self.add_option("--setpref",
                        action="append", type="string",
                        default=[],
                        dest="extraPrefs", metavar="PREF=VALUE",
                        help="defines an extra user preference")

        self.set_defaults(**defaults)

    def verifyCommonOptions(self, options, reftest):
        if options.totalChunks is not None and options.thisChunk is None:
            self.error("thisChunk must be specified when totalChunks is specified")

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

        if options.runTestsInParallel:
            if options.logFile is not None:
                self.error("cannot specify logfile with parallel tests")
            if options.totalChunks is not None and options.thisChunk is None:
                self.error("cannot specify thisChunk or totalChunks with parallel tests")
            if options.focusFilterMode != "all":
                self.error("cannot specify focusFilterMode with parallel tests")
            if options.debugger is not None:
                self.error("cannot specify a debugger with parallel tests")

        options.leakThresholds = {
            "default": options.defaultLeakThreshold,
            "tab": 5000,  # See dependencies of bug 1051230.
        }

        return options


def main():
    parser = ReftestOptions()
    reftest = RefTest()

    options, args = parser.parse_args()
    if len(args) != 1:
        print >>sys.stderr, "No reftest.list specified."
        sys.exit(1)

    options = parser.verifyCommonOptions(options, reftest)
    if options.app is None:
        parser.error("could not find the application path, --appname must be specified")

    options.app = reftest.getFullPath(options.app)
    if not os.path.exists(options.app):
        print """Error: Path %(app)s doesn't exist.
Are you executing $objdir/_tests/reftest/runreftest.py?""" \
                % {"app": options.app}
        sys.exit(1)

    if options.xrePath is None:
        options.xrePath = os.path.dirname(options.app)

    if options.symbolsPath and len(urlparse(options.symbolsPath).scheme) < 2:
        options.symbolsPath = reftest.getFullPath(options.symbolsPath)
    options.utilityPath = reftest.getFullPath(options.utilityPath)

    sys.exit(reftest.runTests(args[0], options))

if __name__ == "__main__":
    main()
