# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the reftest test harness.
"""

import collections
import json
import multiprocessing
import os
import re
import shutil
import signal
import subprocess
import sys
import threading

SCRIPT_DIRECTORY = os.path.abspath(
    os.path.realpath(os.path.dirname(__file__)))
if SCRIPT_DIRECTORY not in sys.path:
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

import reftestcommandline

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

class ReftestResolver(object):
    def defaultManifest(self, suite):
        return {"reftest": "reftest.list",
                "crashtest": "crashtests.list",
                "jstestbrowser": "jstests.list"}[suite]

    def findManifest(self, suite, test_file):
        """Return a tuple of (manifest-path, filter-string) for running test_file.

        test_file is a path to a test or a manifest file
        """
        if not os.path.isabs(test_file):
            test_file = self.absManifestPath(test_file)

        if os.path.isdir(test_file):
            return os.path.join(test_file, self.defaultManifest(suite)), None

        if test_file.endswith('.list'):
            return test_file, None

        return (self.findManifest(suite, os.path.dirname(test_file))[0],
                r".*(?:/|\\)%s$" % os.path.basename(test_file))

    def absManifestPath(self, path):
        return os.path.abspath(path)

    def manifestURL(self, options, path):
        return "file://%s" % path

    def resolveManifests(self, options, tests):
        suite = options.suite
        manifests = {}
        for testPath in tests:
            manifest, filter_str = self.findManifest(suite, testPath)
            manifest = self.manifestURL(options, manifest)
            if manifest not in manifests:
                manifests[manifest] = set()
            if filter_str is not None:
                manifests[manifest].add(filter_str)

        for key in manifests.iterkeys():
            if os.path.split(key)[1] != self.defaultManifest(suite):
                print >> sys.stderr, "Invalid manifest for suite %s, %s" %(options.suite, key)
                sys.exit(1)
            manifests[key] = sorted(list(manifests[key]))

        return manifests

class RefTest(object):
    oldcwd = os.getcwd()
    resolver_cls = ReftestResolver

    def __init__(self):
        self.update_mozinfo()
        self.lastTestSeen = 'reftest'
        self.haveDumpedScreen = False
        self.resolver = self.resolver_cls()

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

    def createReftestProfile(self, options, manifests, server='localhost', port=0,
                             profile_to_clone=None):
        """Sets up a profile for reftest.

        :param options: Object containing command line options
        :param manifests: Dictionary of the form {manifest_path: [filters]}
        :param server: Server name to use for http tests
        :param profile_to_clone: Path to a profile to use as the basis for the
                                 test profile"""

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
        if options.shuffle:
            prefs['reftest.shuffle'] = True
        prefs['reftest.focusFilterMode'] = options.focusFilterMode
        prefs['reftest.manifests'] = json.dumps(manifests)

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
        addons = [options.reftestExtensionPath]

        if options.specialPowersExtensionPath is not None:
            addons.append(options.specialPowersExtensionPath)
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

    def runTests(self, tests, options, cmdlineArgs=None):
        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing orphan servers before
        # trying to start new ones.
        self.killNamedOrphans('ssltunnel')
        self.killNamedOrphans('xpcshell')

        manifests = self.resolver.resolveManifests(options, tests)
        if options.filter:
            manifests[""] = options.filter

        if not hasattr(options, "runTestsInParallel") or not options.runTestsInParallel:
            return self.runSerialTests(manifests, options, cmdlineArgs)

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

    def runSerialTests(self, manifests, options, cmdlineArgs=None):
        debuggerInfo = None
        if options.debugger:
            debuggerInfo = mozdebug.get_debugger_info(options.debugger, options.debuggerArgs,
                                                      options.debuggerInteractive)

        profileDir = None
        try:
            if cmdlineArgs == None:
                cmdlineArgs = []
            profile = self.createReftestProfile(options, manifests)
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


def run(**kwargs):
    # Mach gives us kwargs; this is a way to turn them back into an
    # options object
    parser = reftestcommandline.DesktopArgumentsParser()
    reftest = RefTest()
    parser.set_defaults(**kwargs)
    options = parser.parse_args(kwargs["tests"])
    parser.validate(options, reftest)
    return reftest.runTests(options.tests, options)


def main():
    parser = reftestcommandline.DesktopArgumentsParser()
    reftest = RefTest()

    options = parser.parse_args()
    parser.validate(options, reftest)

    sys.exit(reftest.runTests(options.tests, options))


if __name__ == "__main__":
    main()
