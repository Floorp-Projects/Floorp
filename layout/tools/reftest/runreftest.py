# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the reftest test harness.
"""

from optparse import OptionParser
import collections
import json
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import threading

SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))
sys.path.insert(0, SCRIPT_DIRECTORY)

from automation import Automation
from automationutils import (
        addCommonOptions,
        isURL,
        processLeakLog
)
import mozdebug
import mozprofile

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

  def __init__(self, automation=None):
    self.automation = automation or Automation()

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

  def createReftestProfile(self, options, manifest, server='localhost',
                           special_powers=True, profile_to_clone=None):
    """
      Sets up a profile for reftest.
      'manifest' is the path to the reftest.list file we want to test with.  This is used in
      the remote subclass in remotereftest.py so we can write it to a preference for the
      bootstrap extension.
    """

    locations = mozprofile.permissions.ServerLocations()
    locations.add_host(server, port=0)
    locations.add_host('<file>', port=0)

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
    # Likewise for safebrowsing.
    prefs['browser.safebrowsing.enabled'] = False
    prefs['browser.safebrowsing.malware.enabled'] = False
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

    if options.e10s:
      prefs['browser.tabs.remote.autostart'] = True

    for v in options.extraPrefs:
      thispref = v.split('=')
      if len(thispref) < 2:
        print "Error: syntax error in --setpref=" + v
        sys.exit(1)
      prefs[thispref[0]] = mozprofile.Preferences.cast(thispref[1].strip())

    # install the reftest extension bits into the profile
    addons = []
    addons.append(os.path.join(SCRIPT_DIRECTORY, "reftest"))

    # I would prefer to use "--install-extension reftest/specialpowers", but that requires tight coordination with
    # release engineering and landing on multiple branches at once.
    if special_powers and (manifest.endswith('crashtests.list') or manifest.endswith('jstests.list')):
      addons.append(os.path.join(SCRIPT_DIRECTORY, 'specialpowers'))
      # SpecialPowers requires insecure automation-only features that we put behind a pref.
      prefs['security.turn_off_all_security_so_that_viruses_can_take_over_this_computer'] = True

    # Install distributed extensions, if application has any.
    distExtDir = os.path.join(options.app[ : options.app.rfind(os.sep)], "distribution", "extensions")
    if os.path.isdir(distExtDir):
      for f in os.listdir(distExtDir):
        addons.append(os.path.join(distExtDir, f))

    # Install custom extensions.
    for f in options.extensionsToInstall:
      addons.append(self.getFullPath(f))

    kwargs = { 'addons': addons,
               'preferences': prefs,
               'locations': locations }
    if profile_to_clone:
        profile = mozprofile.Profile.clone(profile_to_clone, **kwargs)
    else:
        profile = mozprofile.Profile(**kwargs)

    self.copyExtraFilesToProfile(options, profile)
    return profile

  def buildBrowserEnv(self, options, profileDir):
    browserEnv = self.automation.environment(xrePath = options.xrePath, debugger=options.debugger)
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

  def cleanup(self, profileDir):
    if profileDir:
      shutil.rmtree(profileDir, True)

  def runTests(self, testPath, options, cmdlineArgs = None):
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

    # First job is only needs-focus tests.  Remaining jobs are non-needs-focus and chunked.
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
          amount = int(threadMatches.group(attribute) if threadMatches else 0)
          summaryObj[attribute] += amount
        amount = int(threadMatches.group('total') if threadMatches else 0)
        summaryObj['total'] += amount

    print 'REFTEST INFO | Result summary:'
    for (summaryObj, (text, categories)) in zip(summaryObjects, summaryLines):
      details = ', '.join(["%d %s" % (summaryObj[attribute], description) for (attribute, description) in categories])
      print 'REFTEST INFO | ' + text + ': ' + str(summaryObj['total']) + ' (' +  details + ')'

    return int(any(t.retcode != 0 for t in threads))

  def runSerialTests(self, testPath, options, cmdlineArgs = None):
    debuggerInfo = mozdebug.get_debugger_info(options.debugger, options.debuggerArgs,
        options.debuggerInteractive);

    profileDir = None
    try:
      reftestlist = self.getManifestPath(testPath)
      if cmdlineArgs == None:
        cmdlineArgs = ['-reftest', reftestlist]
      profile = self.createReftestProfile(options, reftestlist)
      profileDir = profile.profile # name makes more sense

      # browser environment
      browserEnv = self.buildBrowserEnv(options, profileDir)

      self.automation.log.info("REFTEST INFO | runreftest.py | Running tests: start.\n")
      status = self.automation.runApp(None, browserEnv, options.app, profileDir,
                                 cmdlineArgs,
                                 utilityPath = options.utilityPath,
                                 xrePath=options.xrePath,
                                 debuggerInfo=debuggerInfo,
                                 symbolsPath=options.symbolsPath,
                                 # give the JS harness 30 seconds to deal
                                 # with its own timeouts
                                 timeout=options.timeout + 30.0)
      processLeakLog(self.leakLogFile, options.leakThresholds, options.ignoreMissingLeaks)
      self.automation.log.info("\nREFTEST INFO | runreftest.py | Running tests: end.")
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
        self.automation.log.warning("WARNING | runreftest.py | Failed to copy %s to profile", abspath)
        continue


class ReftestOptions(OptionParser):

  def __init__(self, automation=None):
    self.automation = automation or Automation()
    OptionParser.__init__(self)
    defaults = {}

    # we want to pass down everything from automation.__all__
    addCommonOptions(self,
                     defaults=dict(zip(self.automation.__all__,
                            [getattr(self.automation, x) for x in self.automation.__all__])))
    self.automation.addCommonOptions(self)
    self.add_option("--appname",
                    action = "store", type = "string", dest = "app",
                    default = os.path.join(SCRIPT_DIRECTORY, self.automation.DEFAULT_APP),
                    help = "absolute path to application, overriding default")
    self.add_option("--extra-profile-file",
                    action = "append", dest = "extraProfileFiles",
                    default = [],
                    help = "copy specified files/dirs to testing profile")
    self.add_option("--timeout",
                    action = "store", dest = "timeout", type = "int",
                    default = 5 * 60, # 5 minutes per bug 479518
                    help = "reftest will timeout in specified number of seconds. [default %default s].")
    self.add_option("--leak-threshold",
                    action = "store", type = "int", dest = "defaultLeakThreshold",
                    default = 0,
                    help = "fail if the number of bytes leaked in default "
                           "processes through refcounted objects (or bytes "
                           "in classes with MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) "
                           "is greater than the given number")
    self.add_option("--utility-path",
                    action = "store", type = "string", dest = "utilityPath",
                    default = self.automation.DIST_BIN,
                    help = "absolute path to directory containing utility "
                           "programs (xpcshell, ssltunnel, certutil)")
    defaults["utilityPath"] = self.automation.DIST_BIN

    self.add_option("--total-chunks",
                    type = "int", dest = "totalChunks",
                    help = "how many chunks to split the tests up into")
    defaults["totalChunks"] = None

    self.add_option("--this-chunk",
                    type = "int", dest = "thisChunk",
                    help = "which chunk to run between 1 and --total-chunks")
    defaults["thisChunk"] = None

    self.add_option("--log-file",
                    action = "store", type = "string", dest = "logFile",
                    default = None,
                    help = "file to log output to in addition to stdout")
    defaults["logFile"] = None

    self.add_option("--skip-slow-tests",
                    dest = "skipSlowTests", action = "store_true",
                    help = "skip tests marked as slow when running")
    defaults["skipSlowTests"] = False

    self.add_option("--ignore-window-size",
                    dest = "ignoreWindowSize", action = "store_true",
                    help = "ignore the window size, which may cause spurious failures and passes")
    defaults["ignoreWindowSize"] = False

    self.add_option("--install-extension",
                    action = "append", dest = "extensionsToInstall",
                    help = "install the specified extension in the testing profile. "
                           "The extension file's name should be <id>.xpi where <id> is "
                           "the extension's id as indicated in its install.rdf. "
                           "An optional path can be specified too.")
    defaults["extensionsToInstall"] = []

    self.add_option("--run-tests-in-parallel",
                    action = "store_true", dest = "runTestsInParallel",
                    help = "run tests in parallel if possible")
    self.add_option("--no-run-tests-in-parallel",
                    action = "store_false", dest = "runTestsInParallel",
                    help = "do not run tests in parallel")
    defaults["runTestsInParallel"] = False

    self.add_option("--setenv",
                    action = "append", type = "string",
                    dest = "environment", metavar = "NAME=VALUE",
                    help = "sets the given variable in the application's "
                           "environment")
    defaults["environment"] = []

    self.add_option("--filter",
                    action = "store", type="string", dest = "filter",
                    help = "specifies a regular expression (as could be passed to the JS "
                           "RegExp constructor) to test against URLs in the reftest manifest; "
                           "only test items that have a matching test URL will be run.")
    defaults["filter"] = None

    self.add_option("--shuffle",
                    action = "store_true", dest = "shuffle",
                    help = "run reftests in random order")
    defaults["shuffle"] = False

    self.add_option("--focus-filter-mode",
                    action = "store", type = "string", dest = "focusFilterMode",
                    help = "filters tests to run by whether they require focus. "
                           "Valid values are `all', `needs-focus', or `non-needs-focus'. "
                           "Defaults to `all'.")
    defaults["focusFilterMode"] = "all"

    self.add_option("--e10s",
                    action = "store_true",
                    dest = "e10s",
                    help = "enables content processes")
    defaults["e10s"] = False

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
        self.error("--xre-path '%s' is not a directory" % options.xrePath)
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

    options.leakThresholds = {"default": options.defaultLeakThreshold}
    options.ignoreMissingLeaks = []

    return options

def main():
  automation = Automation()
  parser = ReftestOptions(automation)
  reftest = RefTest(automation)

  options, args = parser.parse_args()
  if len(args) != 1:
    print >>sys.stderr, "No reftest.list specified."
    sys.exit(1)

  options = parser.verifyCommonOptions(options, reftest)

  options.app = reftest.getFullPath(options.app)
  if not os.path.exists(options.app):
    print """Error: Path %(app)s doesn't exist.
Are you executing $objdir/_tests/reftest/runreftest.py?""" \
            % {"app": options.app}
    sys.exit(1)

  if options.xrePath is None:
    options.xrePath = os.path.dirname(options.app)

  if options.symbolsPath and not isURL(options.symbolsPath):
    options.symbolsPath = reftest.getFullPath(options.symbolsPath)
  options.utilityPath = reftest.getFullPath(options.utilityPath)

  sys.exit(reftest.runTests(args[0], options))

if __name__ == "__main__":
  main()
