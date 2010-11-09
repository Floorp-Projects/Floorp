#
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
# The Initial Developer of the Original Code is The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Serge Gautherie <sgautherie.bz@free.fr>
#   Ted Mielczarek <ted.mielczarek@gmail.com>
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
# ***** END LICENSE BLOCK ***** */

import glob, logging, os, platform, shutil, subprocess, sys
import re
from urlparse import urlparse

__all__ = [
  "addCommonOptions",
  "checkForCrashes",
  "dumpLeakLog",
  "isURL",
  "processLeakLog",
  "getDebuggerInfo",
  "DEBUGGER_INFO",
  "replaceBackSlashes",
  "wrapCommand",
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

  # valgrind doesn't explain much about leaks unless you set the
  # '--leak-check=full' flag.
  "valgrind": {
    "interactive": False,
    "args": "--leak-check=full"
  }
}

log = logging.getLogger()

def isURL(thing):
  """Return True if |thing| looks like a URL."""
  return urlparse(thing).scheme != ''

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

def checkForCrashes(dumpDir, symbolsPath, testName=None):
  stackwalkPath = os.environ.get('MINIDUMP_STACKWALK', None)
  stackwalkCGI = os.environ.get('MINIDUMP_STACKWALK_CGI', None)
  # try to get the caller's filename if no test name is given
  if testName is None:
    try:
      testName = os.path.basename(sys._getframe(1).f_code.co_filename)
    except:
      testName = "unknown"

  foundCrash = False
  dumps = glob.glob(os.path.join(dumpDir, '*.dmp'))
  for d in dumps:
    log.info("PROCESS-CRASH | %s | application crashed (minidump found)", testName)
    if symbolsPath and stackwalkPath and os.path.exists(stackwalkPath):
      p = subprocess.Popen([stackwalkPath, d, symbolsPath],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
      (out, err) = p.communicate()
      if len(out) > 3:
        # minidump_stackwalk is chatty, so ignore stderr when it succeeds.
        print out
      else:
        print "stderr from minidump_stackwalk:"
        print err
      if p.returncode != 0:
        print "minidump_stackwalk exited with return code %d" % p.returncode
    elif stackwalkCGI and symbolsPath and isURL(symbolsPath):
      f = None
      try:
        f = open(d, "rb")
        sys.path.append(os.path.join(os.path.dirname(__file__), "poster.zip"))
        from poster.encode import multipart_encode
        from poster.streaminghttp import register_openers
        import urllib2
        register_openers()
        datagen, headers = multipart_encode({"minidump": f,
                                             "symbols": symbolsPath})
        request = urllib2.Request(stackwalkCGI, datagen, headers)
        result = urllib2.urlopen(request).read()
        if len(result) > 3:
          print result
        else:
          print "stackwalkCGI returned nothing."
      finally:
        if f:
          f.close()
    else:
      if not symbolsPath:
        print "No symbols path given, can't process dump."
      if not stackwalkPath and not stackwalkCGI:
        print "Neither MINIDUMP_STACKWALK nor MINIDUMP_STACKWALK_CGI is set, can't process dump."
      else:
        if stackwalkPath and not os.path.exists(stackwalkPath):
          print "MINIDUMP_STACKWALK binary not found: %s" % stackwalkPath
        elif stackwalkCGI and not isURL(stackwalkCGI):
          print "MINIDUMP_STACKWALK_CGI is not a URL: %s" % stackwalkCGI
        elif symbolsPath and not isURL(symbolsPath):
          print "symbolsPath is not a URL: %s" % symbolsPath
    dumpSavePath = os.environ.get('MINIDUMP_SAVE_PATH', None)
    if dumpSavePath:
      shutil.move(d, dumpSavePath)
      print "Saved dump as %s" % os.path.join(dumpSavePath,
                                              os.path.basename(d))
    else:
      os.remove(d)
    extra = os.path.splitext(d)[0] + ".extra"
    if os.path.exists(extra):
      os.remove(extra)
    foundCrash = True

  return foundCrash
  
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
      "args": getDebuggerInfo("args", "").split()
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

  leaks = open(leakLogFile, "r")
  leakReport = leaks.read()
  leaks.close()

  # Only |XPCOM_MEM_LEAK_LOG| reports can be actually filtered out.
  # Only check whether an actual leak was reported.
  if filter and not "0 TOTAL " in leakReport:
    return

  # Simply copy the log.
  log.info(leakReport.rstrip("\n"))

def processSingleLeakFile(leakLogFileName, PID, processType, leakThreshold):
  """Process a single leak log, corresponding to the specified
  process PID and type.
  """

  #                  Per-Inst  Leaked      Total  Rem ...
  #   0 TOTAL              17     192  419115886    2 ...
  # 833 nsTimerImpl        60     120      24726    2 ...
  lineRe = re.compile(r"^\s*\d+\s+(?P<name>\S+)\s+"
                      r"(?P<size>-?\d+)\s+(?P<bytesLeaked>-?\d+)\s+"
                      r"-?\d+\s+(?P<numLeaked>-?\d+)")

  processString = ""
  if PID and processType:
    processString = "| %s process %s " % (processType, PID)
  leaks = open(leakLogFileName, "r")
  for line in leaks:
    matches = lineRe.match(line)
    if (matches and
        int(matches.group("numLeaked")) == 0 and
        matches.group("name") != "TOTAL"):
      continue
    log.info(line.rstrip())
  leaks.close()

  leaks = open(leakLogFileName, "r")
  seenTotal = False
  crashedOnPurpose = False
  prefix = "TEST-PASS"
  numObjects = 0
  for line in leaks:
    if line.find("purposefully crash") > -1:
      crashedOnPurpose = True
    matches = lineRe.match(line)
    if not matches:
      continue
    name = matches.group("name")
    size = int(matches.group("size"))
    bytesLeaked = int(matches.group("bytesLeaked"))
    numLeaked = int(matches.group("numLeaked"))
    if size < 0 or bytesLeaked < 0 or numLeaked < 0:
      log.info("TEST-UNEXPECTED-FAIL %s| automationutils.processLeakLog() | negative leaks caught!" %
               processString)
      if name == "TOTAL":
        seenTotal = True
    elif name == "TOTAL":
      seenTotal = True
      # Check for leaks.
      if bytesLeaked < 0 or bytesLeaked > leakThreshold:
        prefix = "TEST-UNEXPECTED-FAIL"
        leakLog = "TEST-UNEXPECTED-FAIL %s| automationutils.processLeakLog() | leaked" \
                  " %d bytes during test execution" % (processString, bytesLeaked)
      elif bytesLeaked > 0:
        leakLog = "TEST-PASS %s| automationutils.processLeakLog() | WARNING leaked" \
                  " %d bytes during test execution" % (processString, bytesLeaked)
      else:
        leakLog = "TEST-PASS %s| automationutils.processLeakLog() | no leaks detected!" \
                  % processString
      # Remind the threshold if it is not 0, which is the default/goal.
      if leakThreshold != 0:
        leakLog += " (threshold set at %d bytes)" % leakThreshold
      # Log the information.
      log.info(leakLog)
    else:
      if numLeaked != 0:
        if numLeaked > 1:
          instance = "instances"
          rest = " each (%s bytes total)" % matches.group("bytesLeaked")
        else:
          instance = "instance"
          rest = ""
        numObjects += 1
        if numObjects > 5:
          # don't spam brief tinderbox logs with tons of leak output
          prefix = "TEST-INFO"
        log.info("%(prefix)s %(process)s| automationutils.processLeakLog() | leaked %(numLeaked)d %(instance)s of %(name)s "
                 "with size %(size)s bytes%(rest)s" %
                 { "prefix": prefix,
                   "process": processString,
                   "numLeaked": numLeaked,
                   "instance": instance,
                   "name": name,
                   "size": matches.group("size"),
                   "rest": rest })
  if not seenTotal:
    if crashedOnPurpose:
      log.info("INFO | automationutils.processLeakLog() | process %s was " \
               "deliberately crashed and thus has no leak log" % PID)
    else:
      log.info("TEST-UNEXPECTED-FAIL %s| automationutils.processLeakLog() | missing output line for total leaks!" %
             processString)
  leaks.close()


def processLeakLog(leakLogFile, leakThreshold = 0):
  """Process the leak log, including separate leak logs created
  by child processes.

  Use this function if you want an additional PASS/FAIL summary.
  It must be used with the |XPCOM_MEM_BLOAT_LOG| environment variable.
  """

  if not os.path.exists(leakLogFile):
    log.info("WARNING | automationutils.processLeakLog() | refcount logging is off, so leaks can't be detected!")
    return

  (leakLogFileDir, leakFileBase) = os.path.split(leakLogFile)
  pidRegExp = re.compile(r".*?_([a-z]*)_pid(\d*)$")
  if leakFileBase[-4:] == ".log":
    leakFileBase = leakFileBase[:-4]
    pidRegExp = re.compile(r".*?_([a-z]*)_pid(\d*).log$")

  for fileName in os.listdir(leakLogFileDir):
    if fileName.find(leakFileBase) != -1:
      thisFile = os.path.join(leakLogFileDir, fileName)
      processPID = 0
      processType = None
      m = pidRegExp.search(fileName)
      if m:
        processType = m.group(1)
        processPID = m.group(2)
      processSingleLeakFile(thisFile, processPID, processType, leakThreshold)

def replaceBackSlashes(input):
  return input.replace('\\', '/')

def wrapCommand(cmd):
  """
  If running on OS X 10.5 or older, wrap |cmd| so that it will
  be executed as an i386 binary, in case it's a 32-bit/64-bit universal
  binary.
  """
  if platform.system() == "Darwin" and \
     hasattr(platform, 'mac_ver') and \
     platform.mac_ver()[0][:4] < '10.6':
    return ["arch", "-arch", "i386"] + cmd
  # otherwise just execute the command normally
  return cmd
