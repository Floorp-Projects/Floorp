#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import logging
from operator import itemgetter
import os
import platform
import re
import signal
import subprocess
import sys
import tempfile
from urlparse import urlparse
import zipfile
import mozinfo

__all__ = [
  "ZipFileReader",
  "addCommonOptions",
  "dumpLeakLog",
  "isURL",
  "processLeakLog",
  "getDebuggerInfo",
  "DEBUGGER_INFO",
  "replaceBackSlashes",
  'KeyValueParseError',
  'parseKeyValue',
  'systemMemory',
  'environment',
  'dumpScreen',
  "ShutdownLeaks"
  ]

# Map of debugging programs to information about them, like default arguments
# and whether or not they are interactive.
DEBUGGER_INFO = {
  # gdb requires that you supply the '--args' flag in order to pass arguments
  # after the executable name to the executable.
  "gdb": {
    "interactive": True,
    "args": "-q --args"
  },

  "cgdb": {
    "interactive": True,
    "args": "-q --args"
  },

  "lldb": {
    "interactive": True,
    "args": "--",
    "requiresEscapedArgs": True
  },

  # Visual Studio Debugger Support
  "devenv.exe": {
    "interactive": True,
    "args": "-debugexe"
  },

  # Visual C++ Express Debugger Support
  "wdexpress.exe": {
    "interactive": True,
    "args": "-debugexe"
  },

  # valgrind doesn't explain much about leaks unless you set the
  # '--leak-check=full' flag. But there are a lot of objects that are
  # semi-deliberately leaked, so we set '--show-possibly-lost=no' to avoid
  # uninteresting output from those objects. We set '--smc-check==all-non-file'
  # and '--vex-iropt-register-updates=allregs-at-mem-access' so that valgrind
  # deals properly with JIT'd JavaScript code.
  "valgrind": {
    "interactive": False,
    "args": " ".join(["--leak-check=full",
                      "--show-possibly-lost=no",
                      "--smc-check=all-non-file",
                      "--vex-iropt-register-updates=allregs-at-mem-access"])
  }
}

class ZipFileReader(object):
  """
  Class to read zip files in Python 2.5 and later. Limited to only what we
  actually use.
  """

  def __init__(self, filename):
    self._zipfile = zipfile.ZipFile(filename, "r")

  def __del__(self):
    self._zipfile.close()

  def _getnormalizedpath(self, path):
    """
    Gets a normalized path from 'path' (or the current working directory if
    'path' is None). Also asserts that the path exists.
    """
    if path is None:
      path = os.curdir
    path = os.path.normpath(os.path.expanduser(path))
    assert os.path.isdir(path)
    return path

  def _extractname(self, name, path):
    """
    Extracts a file with the given name from the zip file to the given path.
    Also creates any directories needed along the way.
    """
    filename = os.path.normpath(os.path.join(path, name))
    if name.endswith("/"):
      os.makedirs(filename)
    else:
      path = os.path.split(filename)[0]
      if not os.path.isdir(path):
        os.makedirs(path)
      with open(filename, "wb") as dest:
        dest.write(self._zipfile.read(name))

  def namelist(self):
    return self._zipfile.namelist()

  def read(self, name):
    return self._zipfile.read(name)

  def extract(self, name, path = None):
    if hasattr(self._zipfile, "extract"):
      return self._zipfile.extract(name, path)

    # This will throw if name is not part of the zip file.
    self._zipfile.getinfo(name)

    self._extractname(name, self._getnormalizedpath(path))

  def extractall(self, path = None):
    if hasattr(self._zipfile, "extractall"):
      return self._zipfile.extractall(path)

    path = self._getnormalizedpath(path)

    for name in self._zipfile.namelist():
      self._extractname(name, path)

log = logging.getLogger()

def isURL(thing):
  """Return True if |thing| looks like a URL."""
  # We want to download URLs like http://... but not Windows paths like c:\...
  return len(urlparse(thing).scheme) >= 2

# Python does not provide strsignal() even in the very latest 3.x.
# This is a reasonable fake.
def strsig(n):
  # Signal numbers run 0 through NSIG-1; an array with NSIG members
  # has exactly that many slots
  _sigtbl = [None]*signal.NSIG
  for k in dir(signal):
    if k.startswith("SIG") and not k.startswith("SIG_") and k != "SIGCLD" and k != "SIGPOLL":
      _sigtbl[getattr(signal, k)] = k
  # Realtime signals mostly have no names
  if hasattr(signal, "SIGRTMIN") and hasattr(signal, "SIGRTMAX"):
    for r in range(signal.SIGRTMIN+1, signal.SIGRTMAX+1):
      _sigtbl[r] = "SIGRTMIN+" + str(r - signal.SIGRTMIN)
  # Fill in any remaining gaps
  for i in range(signal.NSIG):
    if _sigtbl[i] is None:
      _sigtbl[i] = "unrecognized signal, number " + str(i)
  if n < 0 or n >= signal.NSIG:
    return "out-of-range signal, number "+str(n)
  return _sigtbl[n]

def printstatus(status, name = ""):
  # 'status' is the exit status
  if os.name != 'posix':
    # Windows error codes are easier to look up if printed in hexadecimal
    if status < 0:
      status += 2**32
    print "TEST-INFO | %s: exit status %x\n" % (name, status)
  elif os.WIFEXITED(status):
    print "TEST-INFO | %s: exit %d\n" % (name, os.WEXITSTATUS(status))
  elif os.WIFSIGNALED(status):
    # The python stdlib doesn't appear to have strsignal(), alas
    print "TEST-INFO | {}: killed by {}".format(name,strsig(os.WTERMSIG(status)))
  else:
    # This is probably a can't-happen condition on Unix, but let's be defensive
    print "TEST-INFO | %s: undecodable exit status %04x\n" % (name, status)

def addCommonOptions(parser, defaults={}):
  parser.add_option("--xre-path",
                    action = "store", type = "string", dest = "xrePath",
                    # individual scripts will set a sane default
                    default = None,
                    help = "absolute path to directory containing XRE (probably xulrunner)")
  if 'SYMBOLS_PATH' not in defaults:
    defaults['SYMBOLS_PATH'] = None
  parser.add_option("--symbols-path",
                    action = "store", type = "string", dest = "symbolsPath",
                    default = defaults['SYMBOLS_PATH'],
                    help = "absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")
  parser.add_option("--debugger",
                    action = "store", dest = "debugger",
                    help = "use the given debugger to launch the application")
  parser.add_option("--debugger-args",
                    action = "store", dest = "debuggerArgs",
                    help = "pass the given args to the debugger _before_ "
                           "the application on the command line")
  parser.add_option("--debugger-interactive",
                    action = "store_true", dest = "debuggerInteractive",
                    help = "prevents the test harness from redirecting "
                        "stdout and stderr for interactive debuggers")

def getFullPath(directory, path):
  "Get an absolute path relative to 'directory'."
  return os.path.normpath(os.path.join(directory, os.path.expanduser(path)))

def searchPath(directory, path):
  "Go one step beyond getFullPath and try the various folders in PATH"
  # Try looking in the current working directory first.
  newpath = getFullPath(directory, path)
  if os.path.isfile(newpath):
    return newpath

  # At this point we have to fail if a directory was given (to prevent cases
  # like './gdb' from matching '/usr/bin/./gdb').
  if not os.path.dirname(path):
    for dir in os.environ['PATH'].split(os.pathsep):
      newpath = os.path.join(dir, path)
      if os.path.isfile(newpath):
        return newpath
  return None

def getDebuggerInfo(directory, debugger, debuggerArgs, debuggerInteractive = False):

  debuggerInfo = None

  if debugger:
    debuggerPath = searchPath(directory, debugger)
    if not debuggerPath:
      print "Error: Path %s doesn't exist." % debugger
      sys.exit(1)

    debuggerName = os.path.basename(debuggerPath).lower()

    def getDebuggerInfo(type, default):
      if debuggerName in DEBUGGER_INFO and type in DEBUGGER_INFO[debuggerName]:
        return DEBUGGER_INFO[debuggerName][type]
      return default

    debuggerInfo = {
      "path": debuggerPath,
      "interactive" : getDebuggerInfo("interactive", False),
      "args": getDebuggerInfo("args", "").split(),
      "requiresEscapedArgs": getDebuggerInfo("requiresEscapedArgs", False)
    }

    if debuggerArgs:
      debuggerInfo["args"] = debuggerArgs.split()
    if debuggerInteractive:
      debuggerInfo["interactive"] = debuggerInteractive

  return debuggerInfo


def dumpLeakLog(leakLogFile, filter = False):
  """Process the leak log, without parsing it.

  Use this function if you want the raw log only.
  Use it preferably with the |XPCOM_MEM_LEAK_LOG| environment variable.
  """

  # Don't warn (nor "info") if the log file is not there.
  if not os.path.exists(leakLogFile):
    return

  with open(leakLogFile, "r") as leaks:
    leakReport = leaks.read()

  # Only |XPCOM_MEM_LEAK_LOG| reports can be actually filtered out.
  # Only check whether an actual leak was reported.
  if filter and not "0 TOTAL " in leakReport:
    return

  # Simply copy the log.
  log.info(leakReport.rstrip("\n"))

def processSingleLeakFile(leakLogFileName, processType, leakThreshold):
  """Process a single leak log.
  """

  #                  Per-Inst  Leaked      Total  Rem ...
  #   0 TOTAL              17     192  419115886    2 ...
  # 833 nsTimerImpl        60     120      24726    2 ...
  lineRe = re.compile(r"^\s*\d+\s+(?P<name>\S+)\s+"
                      r"(?P<size>-?\d+)\s+(?P<bytesLeaked>-?\d+)\s+"
                      r"-?\d+\s+(?P<numLeaked>-?\d+)")

  processString = ""
  if processType:
    # eg 'plugin'
    processString = " %s process:" % processType

  crashedOnPurpose = False
  totalBytesLeaked = None
  leakAnalysis = []
  leakedObjectNames = []
  with open(leakLogFileName, "r") as leaks:
    for line in leaks:
      if line.find("purposefully crash") > -1:
        crashedOnPurpose = True
      matches = lineRe.match(line)
      if not matches:
        # eg: the leak table header row
        log.info(line.rstrip())
        continue
      name = matches.group("name")
      size = int(matches.group("size"))
      bytesLeaked = int(matches.group("bytesLeaked"))
      numLeaked = int(matches.group("numLeaked"))
      # Output the raw line from the leak log table if it is the TOTAL row,
      # or is for an object row that has been leaked.
      if numLeaked != 0 or name == "TOTAL":
        log.info(line.rstrip())
      # Analyse the leak log, but output later or it will interrupt the leak table
      if name == "TOTAL":
        totalBytesLeaked = bytesLeaked
      if size < 0 or bytesLeaked < 0 or numLeaked < 0:
        leakAnalysis.append("TEST-UNEXPECTED-FAIL | leakcheck |%s negative leaks caught!"
                            % processString)
        continue
      if name != "TOTAL" and numLeaked != 0:
        leakedObjectNames.append(name)
        leakAnalysis.append("TEST-INFO | leakcheck |%s leaked %d %s (%s bytes)"
                            % (processString, numLeaked, name, bytesLeaked))
  log.info('\n'.join(leakAnalysis))

  if totalBytesLeaked is None:
    # We didn't see a line with name 'TOTAL'
    if crashedOnPurpose:
      log.info("TEST-INFO | leakcheck |%s deliberate crash and thus no leak log"
               % processString)
    else:
      # TODO: This should be a TEST-UNEXPECTED-FAIL, but was changed to a warning
      # due to too many intermittent failures (see bug 831223).
      log.info("WARNING | leakcheck |%s missing output line for total leaks!"
               % processString)
    return

  if totalBytesLeaked == 0:
    log.info("TEST-PASS | leakcheck |%s no leaks detected!" % processString)
    return

  # totalBytesLeaked was seen and is non-zero.
  if totalBytesLeaked > leakThreshold:
    if processType and processType == "tab":
      # For now, ignore tab process leaks. See bug 1051230.
      log.info("WARNING | leakcheck | ignoring leaks in tab process")
      prefix = "WARNING"
    else:
      # Fail the run if we're over the threshold (which defaults to 0)
      prefix = "TEST-UNEXPECTED-FAIL"
  else:
    prefix = "WARNING"
  # Create a comma delimited string of the first N leaked objects found,
  # to aid with bug summary matching in TBPL. Note: The order of the objects
  # had no significance (they're sorted alphabetically).
  maxSummaryObjects = 5
  leakedObjectSummary = ', '.join(leakedObjectNames[:maxSummaryObjects])
  if len(leakedObjectNames) > maxSummaryObjects:
    leakedObjectSummary += ', ...'
  log.info("%s | leakcheck |%s %d bytes leaked (%s)"
           % (prefix, processString, totalBytesLeaked, leakedObjectSummary))

def processLeakLog(leakLogFile, leakThreshold = 0):
  """Process the leak log, including separate leak logs created
  by child processes.

  Use this function if you want an additional PASS/FAIL summary.
  It must be used with the |XPCOM_MEM_BLOAT_LOG| environment variable.
  """

  if not os.path.exists(leakLogFile):
    log.info("WARNING | leakcheck | refcount logging is off, so leaks can't be detected!")
    return

  if leakThreshold != 0:
    log.info("TEST-INFO | leakcheck | threshold set at %d bytes" % leakThreshold)

  (leakLogFileDir, leakFileBase) = os.path.split(leakLogFile)
  fileNameRegExp = re.compile(r".*?_([a-z]*)_pid\d*$")
  if leakFileBase[-4:] == ".log":
    leakFileBase = leakFileBase[:-4]
    fileNameRegExp = re.compile(r".*?_([a-z]*)_pid\d*.log$")

  for fileName in os.listdir(leakLogFileDir):
    if fileName.find(leakFileBase) != -1:
      thisFile = os.path.join(leakLogFileDir, fileName)
      processType = None
      m = fileNameRegExp.search(fileName)
      if m:
        processType = m.group(1)
      processSingleLeakFile(thisFile, processType, leakThreshold)

def replaceBackSlashes(input):
  return input.replace('\\', '/')

class KeyValueParseError(Exception):
  """error when parsing strings of serialized key-values"""
  def __init__(self, msg, errors=()):
    self.errors = errors
    Exception.__init__(self, msg)

def parseKeyValue(strings, separator='=', context='key, value: '):
  """
  parse string-serialized key-value pairs in the form of
  `key = value`. Returns a list of 2-tuples.
  Note that whitespace is not stripped.
  """

  # syntax check
  missing = [string for string in strings if separator not in string]
  if missing:
    raise KeyValueParseError("Error: syntax error in %s" % (context,
                                                            ','.join(missing)),
                                                            errors=missing)
  return [string.split(separator, 1) for string in strings]

def systemMemory():
  """
  Returns total system memory in kilobytes.
  Works only on unix-like platforms where `free` is in the path.
  """
  return int(os.popen("free").readlines()[1].split()[1])

def environment(xrePath, env=None, crashreporter=True, debugger=False, dmdPath=None, lsanPath=None):
  """populate OS environment variables for mochitest"""

  env = os.environ.copy() if env is None else env

  assert os.path.isabs(xrePath)

  ldLibraryPath = xrePath

  envVar = None
  dmdLibrary = None
  preloadEnvVar = None
  if 'toolkit' in mozinfo.info and mozinfo.info['toolkit'] == "gonk":
    # Skip all of this, it's only valid for the host.
    pass
  elif mozinfo.isUnix:
    envVar = "LD_LIBRARY_PATH"
    env['MOZILLA_FIVE_HOME'] = xrePath
    dmdLibrary = "libdmd.so"
    preloadEnvVar = "LD_PRELOAD"
  elif mozinfo.isMac:
    envVar = "DYLD_LIBRARY_PATH"
    dmdLibrary = "libdmd.dylib"
    preloadEnvVar = "DYLD_INSERT_LIBRARIES"
  elif mozinfo.isWin:
    envVar = "PATH"
    dmdLibrary = "dmd.dll"
    preloadEnvVar = "MOZ_REPLACE_MALLOC_LIB"
  if envVar:
    envValue = ((env.get(envVar), str(ldLibraryPath))
                if mozinfo.isWin
                else (ldLibraryPath, dmdPath, env.get(envVar)))
    env[envVar] = os.path.pathsep.join([path for path in envValue if path])

  if dmdPath and dmdLibrary and preloadEnvVar:
    env['DMD'] = '1'
    env[preloadEnvVar] = os.path.join(dmdPath, dmdLibrary)

  # crashreporter
  env['GNOME_DISABLE_CRASH_DIALOG'] = '1'
  env['XRE_NO_WINDOWS_CRASH_DIALOG'] = '1'
  env['NS_TRACE_MALLOC_DISABLE_STACKS'] = '1'

  if crashreporter and not debugger:
    env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
    env['MOZ_CRASHREPORTER'] = '1'
  else:
    env['MOZ_CRASHREPORTER_DISABLE'] = '1'

  # Crash on non-local network connections.
  env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '1'

  # Set WebRTC logging in case it is not set yet
  env.setdefault('NSPR_LOG_MODULES', 'signaling:5,mtransport:5,datachannel:5')
  env.setdefault('R_LOG_LEVEL', '6')
  env.setdefault('R_LOG_DESTINATION', 'stderr')
  env.setdefault('R_LOG_VERBOSE', '1')

  # ASan specific environment stuff
  asan = bool(mozinfo.info.get("asan"))
  if asan and (mozinfo.isLinux or mozinfo.isMac):
    try:
      # Symbolizer support
      llvmsym = os.path.join(xrePath, "llvm-symbolizer")
      if os.path.isfile(llvmsym):
        env["ASAN_SYMBOLIZER_PATH"] = llvmsym
        log.info("INFO | runtests.py | ASan using symbolizer at %s", llvmsym)
      else:
        log.info("TEST-UNEXPECTED-FAIL | runtests.py | Failed to find ASan symbolizer at %s", llvmsym)

      totalMemory = systemMemory()

      # Only 4 GB RAM or less available? Use custom ASan options to reduce
      # the amount of resources required to do the tests. Standard options
      # will otherwise lead to OOM conditions on the current test slaves.
      message = "INFO | runtests.py | ASan running in %s configuration"
      asanOptions = []
      if totalMemory <= 1024 * 1024 * 4:
        message = message % 'low-memory'
        asanOptions = ['quarantine_size=50331648', 'malloc_context_size=5']
      else:
        message = message % 'default memory'

      if lsanPath:
        log.info("LSan enabled.")
        asanOptions.append('detect_leaks=1')
        lsanOptions = ["exitcode=0"]
        suppressionsFile = os.path.join(lsanPath, 'lsan_suppressions.txt')
        if os.path.exists(suppressionsFile):
          log.info("LSan using suppression file " + suppressionsFile)
          lsanOptions.append("suppressions=" + suppressionsFile)
        else:
          log.info("WARNING | runtests.py | LSan suppressions file does not exist! " + suppressionsFile)
        env["LSAN_OPTIONS"] = ':'.join(lsanOptions)
        # Run shutdown GCs and CCs to avoid spurious leaks.
        env['MOZ_CC_RUN_DURING_SHUTDOWN'] = '1'

      if len(asanOptions):
        env['ASAN_OPTIONS'] = ':'.join(asanOptions)

    except OSError,err:
      log.info("Failed determine available memory, disabling ASan low-memory configuration: %s", err.strerror)
    except:
      log.info("Failed determine available memory, disabling ASan low-memory configuration")
    else:
      log.info(message)

  return env

def dumpScreen(utilityPath):
  """dumps a screenshot of the entire screen to a directory specified by
  the MOZ_UPLOAD_DIR environment variable"""

  # Need to figure out which OS-dependent tool to use
  if mozinfo.isUnix:
    utility = [os.path.join(utilityPath, "screentopng")]
    utilityname = "screentopng"
  elif mozinfo.isMac:
    utility = ['/usr/sbin/screencapture', '-C', '-x', '-t', 'png']
    utilityname = "screencapture"
  elif mozinfo.isWin:
    utility = [os.path.join(utilityPath, "screenshot.exe")]
    utilityname = "screenshot"

  # Get dir where to write the screenshot file
  parent_dir = os.environ.get('MOZ_UPLOAD_DIR', None)
  if not parent_dir:
    log.info('Failed to retrieve MOZ_UPLOAD_DIR env var')
    return

  # Run the capture
  try:
    tmpfd, imgfilename = tempfile.mkstemp(prefix='mozilla-test-fail-screenshot_', suffix='.png', dir=parent_dir)
    os.close(tmpfd)
    returncode = subprocess.call(utility + [imgfilename])
    printstatus(returncode, utilityname)
  except OSError, err:
    log.info("Failed to start %s for screenshot: %s",
             utility[0], err.strerror)
    return

class ShutdownLeaks(object):
  """
  Parses the mochitest run log when running a debug build, assigns all leaked
  DOM windows (that are still around after test suite shutdown, despite running
  the GC) to the tests that created them and prints leak statistics.
  """

  def __init__(self, logger):
    self.logger = logger
    self.tests = []
    self.leakedWindows = {}
    self.leakedDocShells = set()
    self.currentTest = None
    self.seenShutdown = False

  def log(self, message):
    if message['action'] == 'log':
        line = message['message']
        if line[2:11] == "DOMWINDOW":
          self._logWindow(line)
        elif line[2:10] == "DOCSHELL":
          self._logDocShell(line)
        elif line.startswith("TEST-START | Shutdown"):
          self.seenShutdown = True
    elif message['action'] == 'test_start':
      fileName = message['test'].replace("chrome://mochitests/content/browser/", "")
      self.currentTest = {"fileName": fileName, "windows": set(), "docShells": set()}
    elif message['action'] == 'test_end':
      # don't track a test if no windows or docShells leaked
      if self.currentTest and (self.currentTest["windows"] or self.currentTest["docShells"]):
        self.tests.append(self.currentTest)
      self.currentTest = None

  def process(self):
    if not self.seenShutdown:
      self.logger("TEST-UNEXPECTED-FAIL | ShutdownLeaks | process() called before end of test suite")

    for test in self._parseLeakingTests():
      for url, count in self._zipLeakedWindows(test["leakedWindows"]):
        self.logger("TEST-UNEXPECTED-FAIL | %s | leaked %d window(s) until shutdown [url = %s]" % (test["fileName"], count, url))

      if test["leakedDocShells"]:
        self.logger("TEST-UNEXPECTED-FAIL | %s | leaked %d docShell(s) until shutdown" % (test["fileName"], len(test["leakedDocShells"])))

  def _logWindow(self, line):
    created = line[:2] == "++"
    pid = self._parseValue(line, "pid")
    serial = self._parseValue(line, "serial")

    # log line has invalid format
    if not pid or not serial:
      self.logger("TEST-UNEXPECTED-FAIL | ShutdownLeaks | failed to parse line <%s>" % line)
      return

    key = pid + "." + serial

    if self.currentTest:
      windows = self.currentTest["windows"]
      if created:
        windows.add(key)
      else:
        windows.discard(key)
    elif self.seenShutdown and not created:
      self.leakedWindows[key] = self._parseValue(line, "url")

  def _logDocShell(self, line):
    created = line[:2] == "++"
    pid = self._parseValue(line, "pid")
    id = self._parseValue(line, "id")

    # log line has invalid format
    if not pid or not id:
      self.logger("TEST-UNEXPECTED-FAIL | ShutdownLeaks | failed to parse line <%s>" % line)
      return

    key = pid + "." + id

    if self.currentTest:
      docShells = self.currentTest["docShells"]
      if created:
        docShells.add(key)
      else:
        docShells.discard(key)
    elif self.seenShutdown and not created:
      self.leakedDocShells.add(key)

  def _parseValue(self, line, name):
    match = re.search("\[%s = (.+?)\]" % name, line)
    if match:
      return match.group(1)
    return None

  def _parseLeakingTests(self):
    leakingTests = []

    for test in self.tests:
      test["leakedWindows"] = [self.leakedWindows[id] for id in test["windows"] if id in self.leakedWindows]
      test["leakedDocShells"] = [id for id in test["docShells"] if id in self.leakedDocShells]
      test["leakCount"] = len(test["leakedWindows"]) + len(test["leakedDocShells"])

      if test["leakCount"]:
        leakingTests.append(test)

    return sorted(leakingTests, key=itemgetter("leakCount"), reverse=True)

  def _zipLeakedWindows(self, leakedWindows):
    counts = []
    counted = set()

    for url in leakedWindows:
      if not url in counted:
        counts.append((url, leakedWindows.count(url)))
        counted.add(url)

    return sorted(counts, key=itemgetter(1), reverse=True)


class LSANLeaks(object):
  """
  Parses the log when running an LSAN build, looking for interesting stack frames
  in allocation stacks, and prints out reports.
  """

  def __init__(self, logger):
    self.logger = logger
    self.inReport = False
    self.foundFrames = set([])
    self.recordMoreFrames = None
    self.currStack = None
    self.maxNumRecordedFrames = 4

    # Don't various allocation-related stack frames, as they do not help much to
    # distinguish different leaks.
    unescapedSkipList = [
      "malloc", "js_malloc", "malloc_", "__interceptor_malloc", "moz_malloc", "moz_xmalloc",
      "calloc", "js_calloc", "calloc_", "__interceptor_calloc", "moz_calloc", "moz_xcalloc",
      "realloc","js_realloc", "realloc_", "__interceptor_realloc", "moz_realloc", "moz_xrealloc",
      "new",
      "js::MallocProvider",
    ]
    self.skipListRegExp = re.compile("^" + "|".join([re.escape(f) for f in unescapedSkipList]) + "$")

    self.startRegExp = re.compile("==\d+==ERROR: LeakSanitizer: detected memory leaks")
    self.stackFrameRegExp = re.compile("    #\d+ 0x[0-9a-f]+ in ([^(</]+)")
    self.sysLibStackFrameRegExp = re.compile("    #\d+ 0x[0-9a-f]+ \(([^+]+)\+0x[0-9a-f]+\)")


  def log(self, line):
    if re.match(self.startRegExp, line):
      self.inReport = True
      return

    if not self.inReport:
      return

    if line.startswith("Direct leak"):
      self._finishStack()
      self.recordMoreFrames = True
      self.currStack = []
      return

    if line.startswith("Indirect leak"):
      self._finishStack()
      # Only report direct leaks, in the hope that they are less flaky.
      self.recordMoreFrames = False
      return

    if line.startswith("SUMMARY: AddressSanitizer"):
      self._finishStack()
      self.inReport = False
      return

    if not self.recordMoreFrames:
      return

    stackFrame = re.match(self.stackFrameRegExp, line)
    if stackFrame:
      # Split the frame to remove any return types.
      frame = stackFrame.group(1).split()[-1]
      if not re.match(self.skipListRegExp, frame):
        self._recordFrame(frame)
      return

    sysLibStackFrame = re.match(self.sysLibStackFrameRegExp, line)
    if sysLibStackFrame:
      # System library stack frames will never match the skip list,
      # so don't bother checking if they do.
      self._recordFrame(sysLibStackFrame.group(1))

    # If we don't match either of these, just ignore the frame.
    # We'll end up with "unknown stack" if everything is ignored.

  def process(self):
    for f in self.foundFrames:
      self.logger("TEST-UNEXPECTED-FAIL | LeakSanitizer | leak at " + f)

  def _finishStack(self):
    if self.recordMoreFrames and len(self.currStack) == 0:
      self.currStack = ["unknown stack"]
    if self.currStack:
      self.foundFrames.add(", ".join(self.currStack))
      self.currStack = None
    self.recordMoreFrames = False
    self.numRecordedFrames = 0

  def _recordFrame(self, frame):
    self.currStack.append(frame)
    self.numRecordedFrames += 1
    if self.numRecordedFrames >= self.maxNumRecordedFrames:
      self.recordMoreFrames = False
