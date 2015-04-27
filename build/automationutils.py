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
import mozinfo

__all__ = [
  "dumpLeakLog",
  "processLeakLog",
  'systemMemory',
  'environment',
  'dumpScreen',
  "ShutdownLeaks",
  "setAutomationLog",
  ]

log = logging.getLogger()
def resetGlobalLog():
  while log.handlers:
    log.removeHandler(log.handlers[0])
  handler = logging.StreamHandler(sys.stdout)
  log.setLevel(logging.INFO)
  log.addHandler(handler)
resetGlobalLog()

def setAutomationLog(alt_logger):
  global log
  log = alt_logger

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

def processSingleLeakFile(leakLogFileName, processType, leakThreshold, ignoreMissingLeaks):
  """Process a single leak log.
  """

  #     |              |Per-Inst  Leaked|     Total  Rem|
  #   0 |TOTAL         |      17     192| 419115886    2|
  # 833 |nsTimerImpl   |      60     120|     24726    2|
  # 930 |Foo<Bar, Bar> |      32       8|       100    1|
  lineRe = re.compile(r"^\s*\d+ \|"
                      r"(?P<name>[^|]+)\|"
                      r"\s*(?P<size>-?\d+)\s+(?P<bytesLeaked>-?\d+)\s*\|"
                      r"\s*-?\d+\s+(?P<numLeaked>-?\d+)")
  # The class name can contain spaces. We remove trailing whitespace later.

  processString = "%s process:" % processType
  crashedOnPurpose = False
  totalBytesLeaked = None
  logAsWarning = False
  leakAnalysis = []
  leakedObjectAnalysis = []
  leakedObjectNames = []
  recordLeakedObjects = False
  with open(leakLogFileName, "r") as leaks:
    for line in leaks:
      if line.find("purposefully crash") > -1:
        crashedOnPurpose = True
      matches = lineRe.match(line)
      if not matches:
        # eg: the leak table header row
        log.info(line.rstrip())
        continue
      name = matches.group("name").rstrip()
      size = int(matches.group("size"))
      bytesLeaked = int(matches.group("bytesLeaked"))
      numLeaked = int(matches.group("numLeaked"))
      # Output the raw line from the leak log table if it is the TOTAL row,
      # or is for an object row that has been leaked.
      if numLeaked != 0 or name == "TOTAL":
        log.info(line.rstrip())
      # Analyse the leak log, but output later or it will interrupt the leak table
      if name == "TOTAL":
        # Multiple default processes can end up writing their bloat views into a single
        # log, particularly on B2G. Eventually, these should be split into multiple
        # logs (bug 1068869), but for now, we report the largest leak.
        if totalBytesLeaked != None:
          leakAnalysis.append("WARNING | leakcheck | %s multiple BloatView byte totals found"
                              % processString)
        else:
          totalBytesLeaked = 0
        if bytesLeaked > totalBytesLeaked:
          totalBytesLeaked = bytesLeaked
          # Throw out the information we had about the previous bloat view.
          leakedObjectNames = []
          leakedObjectAnalysis = []
          recordLeakedObjects = True
        else:
          recordLeakedObjects = False
      if size < 0 or bytesLeaked < 0 or numLeaked < 0:
        leakAnalysis.append("TEST-UNEXPECTED-FAIL | leakcheck | %s negative leaks caught!"
                            % processString)
        logAsWarning = True
        continue
      if name != "TOTAL" and numLeaked != 0 and recordLeakedObjects:
        leakedObjectNames.append(name)
        leakedObjectAnalysis.append("TEST-INFO | leakcheck | %s leaked %d %s (%s bytes)"
                                    % (processString, numLeaked, name, bytesLeaked))

  leakAnalysis.extend(leakedObjectAnalysis)
  if logAsWarning:
    log.warning('\n'.join(leakAnalysis))
  else:
    log.info('\n'.join(leakAnalysis))

  logAsWarning = False

  if totalBytesLeaked is None:
    # We didn't see a line with name 'TOTAL'
    if crashedOnPurpose:
      log.info("TEST-INFO | leakcheck | %s deliberate crash and thus no leak log"
               % processString)
    elif ignoreMissingLeaks:
      log.info("TEST-INFO | leakcheck | %s ignoring missing output line for total leaks"
               % processString)
    else:
      log.info("TEST-UNEXPECTED-FAIL | leakcheck | %s missing output line for total leaks!"
               % processString)
      log.info("TEST-INFO | leakcheck | missing output line from log file %s"
               % leakLogFileName)
    return

  if totalBytesLeaked == 0:
    log.info("TEST-PASS | leakcheck | %s no leaks detected!" % processString)
    return

  # totalBytesLeaked was seen and is non-zero.
  if totalBytesLeaked > leakThreshold:
    logAsWarning = True
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

  if logAsWarning:
    log.warning("%s | leakcheck | %s %d bytes leaked (%s)"
                % (prefix, processString, totalBytesLeaked, leakedObjectSummary))
  else:
    log.info("%s | leakcheck | %s %d bytes leaked (%s)"
             % (prefix, processString, totalBytesLeaked, leakedObjectSummary))

def processLeakLog(leakLogFile, options):
  """Process the leak log, including separate leak logs created
  by child processes.

  Use this function if you want an additional PASS/FAIL summary.
  It must be used with the |XPCOM_MEM_BLOAT_LOG| environment variable.

  The base of leakLogFile for a non-default process needs to end with
    _proctype_pid12345.log
  "proctype" is a string denoting the type of the process, which should
  be the result of calling XRE_ChildProcessTypeToString(). 12345 is
  a series of digits that is the pid for the process. The .log is
  optional.

  All other file names are treated as being for default processes.

  The options argument is checked for two optional attributes,
  leakThresholds and ignoreMissingLeaks.

  leakThresholds should be a dict mapping process types to leak thresholds,
  in bytes. If a process type is not present in the dict the threshold
  will be 0.

  ignoreMissingLeaks should be a list of process types. If a process
  creates a leak log without a TOTAL, then we report an error if it isn't
  in the list ignoreMissingLeaks.
  """

  if not os.path.exists(leakLogFile):
    log.info("WARNING | leakcheck | refcount logging is off, so leaks can't be detected!")
    return

  leakThresholds = getattr(options, 'leakThresholds', {})
  ignoreMissingLeaks = getattr(options, 'ignoreMissingLeaks', [])

  # This list is based on kGeckoProcessTypeString. ipdlunittest processes likely
  # are not going to produce leak logs we will ever see.
  knownProcessTypes = ["default", "plugin", "tab", "geckomediaplugin"]

  for processType in knownProcessTypes:
    log.info("TEST-INFO | leakcheck | %s process: leak threshold set at %d bytes"
             % (processType, leakThresholds.get(processType, 0)))

  for processType in leakThresholds:
    if not processType in knownProcessTypes:
      log.info("TEST-UNEXPECTED-FAIL | leakcheck | Unknown process type %s in leakThresholds"
               % processType)

  (leakLogFileDir, leakFileBase) = os.path.split(leakLogFile)
  if leakFileBase[-4:] == ".log":
    leakFileBase = leakFileBase[:-4]
    fileNameRegExp = re.compile(r"_([a-z]*)_pid\d*.log$")
  else:
    fileNameRegExp = re.compile(r"_([a-z]*)_pid\d*$")

  for fileName in os.listdir(leakLogFileDir):
    if fileName.find(leakFileBase) != -1:
      thisFile = os.path.join(leakLogFileDir, fileName)
      m = fileNameRegExp.search(fileName)
      if m:
        processType = m.group(1)
      else:
        processType = "default"
      if not processType in knownProcessTypes:
        log.info("TEST-UNEXPECTED-FAIL | leakcheck | Leak log with unknown process type %s"
                 % processType)
      leakThreshold = leakThresholds.get(processType, 0)
      processSingleLeakFile(thisFile, processType, leakThreshold,
                            processType in ignoreMissingLeaks)

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

  if mozinfo.isMac:
    ldLibraryPath = os.path.join(os.path.dirname(xrePath), "MacOS")
  else:
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
    env[preloadEnvVar] = os.path.join(dmdPath, dmdLibrary)

  # crashreporter
  env['GNOME_DISABLE_CRASH_DIALOG'] = '1'
  env['XRE_NO_WINDOWS_CRASH_DIALOG'] = '1'

  if crashreporter and not debugger:
    env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
    env['MOZ_CRASHREPORTER'] = '1'
  else:
    env['MOZ_CRASHREPORTER_DISABLE'] = '1'

  # Crash on non-local network connections by default.
  # MOZ_DISABLE_NONLOCAL_CONNECTIONS can be set to "0" to temporarily
  # enable non-local connections for the purposes of local testing.  Don't
  # override the user's choice here.  See bug 1049688.
  env.setdefault('MOZ_DISABLE_NONLOCAL_CONNECTIONS', '1')

  # Set WebRTC logging in case it is not set yet
  env.setdefault('NSPR_LOG_MODULES', 'signaling:5,mtransport:5,datachannel:5,jsep:5,MediaPipelineFactory:5')
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
        log.info("INFO | runtests.py | ASan using symbolizer at %s" % llvmsym)
      else:
        log.info("TEST-UNEXPECTED-FAIL | runtests.py | Failed to find ASan symbolizer at %s" % llvmsym)

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
      log.info("Failed determine available memory, disabling ASan low-memory configuration: %s" % err.strerror)
    except:
      log.info("Failed determine available memory, disabling ASan low-memory configuration")
    else:
      log.info(message)

  tsan = bool(mozinfo.info.get("tsan"))
  if tsan and mozinfo.isLinux:
    # Symbolizer support.
    llvmsym = os.path.join(xrePath, "llvm-symbolizer")
    if os.path.isfile(llvmsym):
      env["TSAN_OPTIONS"] = "external_symbolizer_path=%s" % llvmsym
      log.info("INFO | runtests.py | TSan using symbolizer at %s" % llvmsym)
    else:
      log.info("TEST-UNEXPECTED-FAIL | runtests.py | Failed to find TSan symbolizer at %s" % llvmsym)

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
    log.info("Failed to start %s for screenshot: %s" %
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
    self.seenShutdown = set()

  def log(self, message):
    if message['action'] == 'log':
        line = message['message']
        if line[2:11] == "DOMWINDOW":
          self._logWindow(line)
        elif line[2:10] == "DOCSHELL":
          self._logDocShell(line)
        elif line.startswith("Completed ShutdownLeaks collections in process"):
          pid = int(line.split()[-1])
          self.seenShutdown.add(pid)
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
      self.logger.warning("TEST-UNEXPECTED-FAIL | ShutdownLeaks | process() called before end of test suite")

    for test in self._parseLeakingTests():
      for url, count in self._zipLeakedWindows(test["leakedWindows"]):
        self.logger.warning("TEST-UNEXPECTED-FAIL | %s | leaked %d window(s) until shutdown [url = %s]" % (test["fileName"], count, url))

      if test["leakedWindowsString"]:
        self.logger.info("TEST-INFO | %s | windows(s) leaked: %s" % (test["fileName"], test["leakedWindowsString"]))

      if test["leakedDocShells"]:
        self.logger.warning("TEST-UNEXPECTED-FAIL | %s | leaked %d docShell(s) until shutdown" % (test["fileName"], len(test["leakedDocShells"])))
        self.logger.info("TEST-INFO | %s | docShell(s) leaked: %s" % (test["fileName"],
                                                                      ', '.join(["[pid = %s] [id = %s]" % x for x in test["leakedDocShells"]])))

  def _logWindow(self, line):
    created = line[:2] == "++"
    pid = self._parseValue(line, "pid")
    serial = self._parseValue(line, "serial")

    # log line has invalid format
    if not pid or not serial:
      self.logger.warning("TEST-UNEXPECTED-FAIL | ShutdownLeaks | failed to parse line <%s>" % line)
      return

    key = (pid, serial)

    if self.currentTest:
      windows = self.currentTest["windows"]
      if created:
        windows.add(key)
      else:
        windows.discard(key)
    elif int(pid) in self.seenShutdown and not created:
      self.leakedWindows[key] = self._parseValue(line, "url")

  def _logDocShell(self, line):
    created = line[:2] == "++"
    pid = self._parseValue(line, "pid")
    id = self._parseValue(line, "id")

    # log line has invalid format
    if not pid or not id:
      self.logger.warning("TEST-UNEXPECTED-FAIL | ShutdownLeaks | failed to parse line <%s>" % line)
      return

    key = (pid, id)

    if self.currentTest:
      docShells = self.currentTest["docShells"]
      if created:
        docShells.add(key)
      else:
        docShells.discard(key)
    elif int(pid) in self.seenShutdown and not created:
      self.leakedDocShells.add(key)

  def _parseValue(self, line, name):
    match = re.search("\[%s = (.+?)\]" % name, line)
    if match:
      return match.group(1)
    return None

  def _parseLeakingTests(self):
    leakingTests = []

    for test in self.tests:
      leakedWindows = [id for id in test["windows"] if id in self.leakedWindows]
      test["leakedWindows"] = [self.leakedWindows[id] for id in leakedWindows]
      test["leakedWindowsString"] = ', '.join(["[pid = %s] [serial = %s]" % x for x in leakedWindows])
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
      "malloc", "js_malloc", "malloc_", "__interceptor_malloc", "moz_xmalloc",
      "calloc", "js_calloc", "calloc_", "__interceptor_calloc", "moz_xcalloc",
      "realloc","js_realloc", "realloc_", "__interceptor_realloc", "moz_xrealloc",
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
      self.logger.warning("TEST-UNEXPECTED-FAIL | LeakSanitizer | leak at " + f)

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
