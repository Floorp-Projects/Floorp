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
import platform
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
import mozlog
import mozprocess
import mozprofile
import mozrunner
from mozrunner.utils import get_stack_fixer_function, test_environment
from mozscreenshot import printstatus, dump_screen

try:
    from marionette_driver.addons import Addons
    from marionette_harness import Marionette
except ImportError, e:
    # Defer ImportError until attempt to use Marionette
    def reraise(*args, **kwargs):
        raise(e)
    Marionette = reraise

from output import OutputHandler, ReftestFormatter
import reftestcommandline

here = os.path.abspath(os.path.dirname(__file__))

try:
    from mozbuild.base import MozbuildObject
    build_obj = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build_obj = None


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
    def __init__(self, cmdargs):
        threading.Thread.__init__(self)
        self.cmdargs = cmdargs
        self.summaryMatches = {}
        self.retcode = -1
        for text, _ in summaryLines:
            self.summaryMatches[text] = None

    def run(self):
        with printLock:
            print "Starting thread with", self.cmdargs
            sys.stdout.flush()
        process = subprocess.Popen(self.cmdargs, stdout=subprocess.PIPE)
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

    def directoryManifest(self, suite, path):
        return os.path.join(path, self.defaultManifest(suite))

    def findManifest(self, suite, test_file, subdirs=True):
        """Return a tuple of (manifest-path, filter-string) for running test_file.

        test_file is a path to a test or a manifest file
        """
        rv = []
        default_manifest = self.defaultManifest(suite)
        if not os.path.isabs(test_file):
            test_file = self.absManifestPath(test_file)

        if os.path.isdir(test_file):
            for dirpath, dirnames, filenames in os.walk(test_file):
                if default_manifest in filenames:
                    rv.append((os.path.join(dirpath, default_manifest), None))
                    # We keep recursing into subdirectories which means that in the case
                    # of include directives we get the same manifest multiple times.
                    # However reftest.js will only read each manifest once

        elif test_file.endswith('.list'):
            if os.path.exists(test_file):
                rv = [(test_file, None)]
        else:
            dirname, pathname = os.path.split(test_file)
            found = True
            while not os.path.exists(os.path.join(dirname, default_manifest)):
                dirname, suffix = os.path.split(dirname)
                pathname = os.path.join(suffix, pathname)
                if os.path.dirname(dirname) == dirname:
                    found = False
                    break
            if found:
                rv = [(os.path.join(dirname, default_manifest),
                       r".*(?:/|\\)%s(?:[#?].*)?$" % pathname)]

        return rv

    def absManifestPath(self, path):
        return os.path.normpath(os.path.abspath(path))

    def manifestURL(self, options, path):
        return "file://%s" % path

    def resolveManifests(self, options, tests):
        suite = options.suite
        manifests = {}
        for testPath in tests:
            for manifest, filter_str in self.findManifest(suite, testPath):
                manifest = self.manifestURL(options, manifest)
                if manifest not in manifests:
                    manifests[manifest] = set()
                manifests[manifest].add(filter_str)

        for key in manifests.iterkeys():
            if None in manifests[key]:
                manifests[key] = None
            else:
                manifests[key] = "|".join(list(manifests[key]))
        return manifests


class RefTest(object):
    use_marionette = True
    oldcwd = os.getcwd()
    resolver_cls = ReftestResolver

    def __init__(self):
        self.update_mozinfo()
        self.lastTestSeen = 'reftest'
        self.haveDumpedScreen = False
        self.resolver = self.resolver_cls()
        self.log = None

    def _populate_logger(self, options):
        if self.log:
            return

        mozlog.commandline.log_formatters["tbpl"] = (ReftestFormatter,
                                                     "Reftest specific formatter for the"
                                                     "benefit of legacy log parsers and"
                                                     "tools such as the reftest analyzer")
        fmt_options = {}
        if not options.log_tbpl_level and os.environ.get('MOZ_REFTEST_VERBOSE'):
            options.log_tbpl_level = fmt_options['level'] = 'debug'
        self.log = mozlog.commandline.setup_logging(
            "reftest harness", options, {"tbpl": sys.stdout}, fmt_options)

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
                             profile_to_clone=None, startAfter=None):
        """Sets up a profile for reftest.

        :param options: Object containing command line options
        :param manifests: Dictionary of the form {manifest_path: [filters]}
        :param server: Server name to use for http tests
        :param profile_to_clone: Path to a profile to use as the basis for the
                                 test profile
        """

        locations = mozprofile.permissions.ServerLocations()
        locations.add_host(server, scheme='http', port=port)
        locations.add_host(server, scheme='https', port=port)

        # Set preferences for communication between our command line arguments
        # and the reftest harness.  Preferences that are required for reftest
        # to work should instead be set in reftest-preferences.js .
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
        if options.repeat:
            prefs['reftest.repeat'] = options.repeat
        if options.runUntilFailure:
            prefs['reftest.runUntilFailure'] = True
        if options.cleanupCrashes:
            prefs['reftest.cleanupPendingCrashes'] = True
        prefs['reftest.focusFilterMode'] = options.focusFilterMode
        prefs['reftest.logLevel'] = options.log_tbpl_level or 'info'
        prefs['reftest.manifests'] = json.dumps(manifests)

        if startAfter is not None:
            prefs['reftest.startAfter'] = startAfter

        if options.e10s:
            prefs['browser.tabs.remote.autostart'] = True
            prefs['extensions.e10sBlocksEnabling'] = False

        # Bug 1262954: For winXP + e10s disable acceleration
        if platform.system() in ("Windows", "Microsoft") and \
           '5.1' in platform.version() and options.e10s:
            prefs['layers.acceleration.disabled'] = True

        # Bug 1300355: Disable canvas cache for win7 as it uses
        # too much memory and causes OOMs.
        if platform.system() in ("Windows", "Microsoft") and \
           '6.1' in platform.version():
            prefs['reftest.nocache'] = True

        if options.marionette:
            port = options.marionette.split(':')[1]
            prefs['marionette.defaultPrefs.port'] = int(port)

        preference_file = os.path.join(here, 'reftest-preferences.js')
        prefs.update(mozprofile.Preferences.read_prefs(preference_file))

        for v in options.extraPrefs:
            thispref = v.split('=')
            if len(thispref) < 2:
                print "Error: syntax error in --setpref=" + v
                sys.exit(1)
            prefs[thispref[0]] = thispref[1].strip()

        addons = []
        if not self.use_marionette:
            addons.append(options.reftestExtensionPath)

        if options.specialPowersExtensionPath is not None:
            if not self.use_marionette:
                addons.append(options.specialPowersExtensionPath)

        for pref in prefs:
            prefs[pref] = mozprofile.Preferences.cast(prefs[pref])

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

        if os.path.join(here, 'chrome') not in options.extraProfileFiles:
            options.extraProfileFiles.append(os.path.join(here, 'chrome'))

        self.copyExtraFilesToProfile(options, profile)
        return profile

    def environment(self, **kwargs):
        kwargs['log'] = self.log
        return test_environment(**kwargs)

    def buildBrowserEnv(self, options, profileDir):
        browserEnv = self.environment(
            xrePath=options.xrePath, debugger=options.debugger)
        browserEnv["XPCOM_DEBUG_BREAK"] = "stack"
        if hasattr(options, "topsrcdir"):
            browserEnv["MOZ_DEVELOPER_REPO_DIR"] = options.topsrcdir

        if mozinfo.info["asan"]:
            # Disable leak checking for reftests for now
            if "ASAN_OPTIONS" in browserEnv:
                browserEnv["ASAN_OPTIONS"] += ":detect_leaks=0"
            else:
                browserEnv["ASAN_OPTIONS"] = "detect_leaks=0"

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
        self.log.info("Checking for orphan %s processes..." % pname)

        def _psInfo(line):
            if pname in line:
                self.log.info(line)
        process = mozprocess.ProcessHandler(['ps', '-f'],
                                            processOutputLine=_psInfo)
        process.run()
        process.wait()

        def _psKill(line):
            parts = line.split()
            if len(parts) == 3 and parts[0].isdigit():
                pid = int(parts[0])
                if parts[2] == pname and parts[1] == '1':
                    self.log.info("killing %s orphan with pid %d" % (pname, pid))
                    try:
                        os.kill(
                            pid, getattr(signal, "SIGKILL", signal.SIGTERM))
                    except Exception as e:
                        self.log.info("Failed to kill process %d: %s" %
                                      (pid, str(e)))
        process = mozprocess.ProcessHandler(['ps', '-o', 'pid,ppid,comm'],
                                            processOutputLine=_psKill)
        process.run()
        process.wait()

    def cleanup(self, profileDir):
        if profileDir:
            shutil.rmtree(profileDir, True)

    def runTests(self, tests, options, cmdargs=None):
        cmdargs = cmdargs or []
        self._populate_logger(options)

        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing orphan servers before
        # trying to start new ones.
        self.killNamedOrphans('ssltunnel')
        self.killNamedOrphans('xpcshell')

        if options.cleanupCrashes:
            mozcrash.cleanup_pending_crash_reports()

        manifests = self.resolver.resolveManifests(options, tests)
        if options.filter:
            manifests[""] = options.filter

        if not getattr(options, 'runTestsInParallel', False):
            return self.runSerialTests(manifests, options, cmdargs)

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

        host = 'localhost'
        port = 2828
        if options.marionette:
            host, port = options.marionette.split(':')

        # First job is only needs-focus tests.  Remaining jobs are
        # non-needs-focus and chunked.
        perProcessArgs[0].insert(-1, "--focus-filter-mode=needs-focus")
        for (chunkNumber, jobArgs) in enumerate(perProcessArgs[1:], start=1):
            jobArgs[-1:-1] = ["--focus-filter-mode=non-needs-focus",
                              "--total-chunks=%d" % jobsWithoutFocus,
                              "--this-chunk=%d" % chunkNumber,
                              "--marionette=%s:%d" % (host, port)]
            port += 1

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
        self.log.error("%s | application timed out after %d seconds with no output" % (
                       self.lastTestSeen, int(timeout)))
        self.log.error("Force-terminating active process(es).")
        self.killAndGetStack(
            proc, utilityPath, debuggerInfo, dump_screen=not debuggerInfo)

    def dumpScreen(self, utilityPath):
        if self.haveDumpedScreen:
            self.log.info("Not taking screenshot here: see the one that was previously logged")
            return
        self.haveDumpedScreen = True
        dump_screen(utilityPath, self.log)

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
                    self.log.info("Can't trigger Breakpad, process no longer exists")
                return
        self.log.info("Can't trigger Breakpad, just killing process")
        process.kill()

    def runApp(self, profile, binary, cmdargs, env,
               timeout=None, debuggerInfo=None,
               symbolsPath=None, options=None,
               valgrindPath=None, valgrindArgs=None, valgrindSuppFiles=None):

        def timeoutHandler():
            self.handleTimeout(
                timeout, proc, options.utilityPath, debuggerInfo)

        interactive = False
        debug_args = None
        if debuggerInfo:
            interactive = debuggerInfo.interactive
            debug_args = [debuggerInfo.path] + debuggerInfo.args

        def record_last_test(message):
            """Records the last test seen by this harness for the benefit of crash logging."""
            if message['action'] == 'test_start':
                if " " in message['test']:
                    self.lastTestSeen = message['test'].split(" ")[0]
                else:
                    self.lastTestSeen = message['test']

        self.log.add_handler(record_last_test)

        outputHandler = OutputHandler(self.log, options.utilityPath, symbolsPath=symbolsPath)

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

        if self.use_marionette:
            marionette_args = {
                'socket_timeout': options.marionette_socket_timeout,
                'startup_timeout': options.marionette_startup_timeout,
                'symbols_path': options.symbolsPath,
            }
            if options.marionette:
                host, port = options.marionette.split(':')
                marionette_args['host'] = host
                marionette_args['port'] = int(port)

            marionette = Marionette(**marionette_args)
            marionette.start_session(timeout=options.marionette_port_timeout)

            addons = Addons(marionette)
            if options.specialPowersExtensionPath:
                addons.install(options.specialPowersExtensionPath, temp=True)

            addons.install(options.reftestExtensionPath, temp=True)

            marionette.delete_session()

        status = runner.wait()
        runner.process_handler = None

        if status:
            msg = "TEST-UNEXPECTED-FAIL | %s | application terminated with exit code %s" % \
                (self.lastTestSeen, status)
            # use process_output so message is logged verbatim
            self.log.process_output(None, msg)
        else:
            self.lastTestSeen = 'Main app process exited normally'

        crashed = mozcrash.log_crashes(self.log, os.path.join(profile.profile, 'minidumps'),
                                       symbolsPath, test=self.lastTestSeen)

        runner.cleanup()
        if not status and crashed:
            status = 1
        return status, self.lastTestSeen

    def runSerialTests(self, manifests, options, cmdargs=None):
        debuggerInfo = None
        if options.debugger:
            debuggerInfo = mozdebug.get_debugger_info(options.debugger, options.debuggerArgs,
                                                      options.debuggerInteractive)

        profileDir = None
        startAfter = None  # When the previous run crashed, we skip the tests we ran before
        prevStartAfter = None
        status = 1  # Just to start the loop
        while status != 0:
            try:
                if cmdargs is None:
                    cmdargs = []

                if self.use_marionette:
                    cmdargs.append('-marionette')

                profile = self.createReftestProfile(options,
                                                    manifests,
                                                    startAfter=startAfter)
                profileDir = profile.profile  # name makes more sense

                # browser environment
                browserEnv = self.buildBrowserEnv(options, profileDir)

                self.log.info("Running with e10s: {}".format(options.e10s))
                status, startAfter = self.runApp(profile,
                                                 binary=options.app,
                                                 cmdargs=cmdargs,
                                                 # give the JS harness 30 seconds to deal with
                                                 # its own timeouts
                                                 env=browserEnv,
                                                 timeout=options.timeout + 30.0,
                                                 symbolsPath=options.symbolsPath,
                                                 options=options,
                                                 debuggerInfo=debuggerInfo)
                self.log.info("Process mode: {}".format('e10s' if options.e10s else 'non-e10s'))
                mozleak.process_leak_log(self.leakLogFile,
                                         leak_thresholds=options.leakThresholds,
                                         stack_fixer=get_stack_fixer_function(options.utilityPath,
                                                                              options.symbolsPath))
                self.cleanup(profileDir)
                if startAfter is not None and options.shuffle:
                    self.log.error("Can not resume from a crash with --shuffle "
                                   "enabled. Please consider disabling --shuffle")
                    break
                if startAfter == prevStartAfter:
                    # If the test stuck on the same test, or there the crashed
                    # test appeared more then once, stop
                    self.log.error("Force stop because we keep running into "
                                   "test \"{}\"".format(startAfter))
                    break
                prevStartAfter = startAfter
                # TODO: we need to emit an SUITE-END log if it crashed
            finally:
                self.cleanup(profileDir)
        return status

    def copyExtraFilesToProfile(self, options, profile):
        "Copy extra files or dirs specified on the command line to the testing profile."
        profileDir = profile.profile
        if not os.path.exists(os.path.join(profileDir, "hyphenation")):
            os.makedirs(os.path.join(profileDir, "hyphenation"))
        for f in options.extraProfileFiles:
            abspath = self.getFullPath(f)
            if os.path.isfile(abspath):
                if os.path.basename(abspath) == 'user.js':
                    extra_prefs = mozprofile.Preferences.read_prefs(abspath)
                    profile.set_preferences(extra_prefs)
                elif os.path.basename(abspath).endswith('.dic'):
                    shutil.copy2(abspath, os.path.join(profileDir, "hyphenation"))
                else:
                    shutil.copy2(abspath, profileDir)
            elif os.path.isdir(abspath):
                dest = os.path.join(profileDir, os.path.basename(abspath))
                shutil.copytree(abspath, dest)
            else:
                self.log.warning(
                    "runreftest.py | Failed to copy %s to profile" % abspath)
                continue


def run_test_harness(parser, options):
    reftest = RefTest()
    parser.validate(options, reftest)

    # We have to validate options.app here for the case when the mach
    # command is able to find it after argument parsing. This can happen
    # when running from a tests.zip.
    if not options.app:
        parser.error("could not find the application path, --appname must be specified")

    options.app = reftest.getFullPath(options.app)
    if not os.path.exists(options.app):
        parser.error("Error: Path %(app)s doesn't exist. Are you executing "
                     "$objdir/_tests/reftest/runreftest.py?" % {"app": options.app})

    if options.xrePath is None:
        options.xrePath = os.path.dirname(options.app)

    return reftest.runTests(options.tests, options)


if __name__ == "__main__":
    parser = reftestcommandline.DesktopArgumentsParser()
    options = parser.parse_args()
    sys.exit(run_test_harness(parser, options))
