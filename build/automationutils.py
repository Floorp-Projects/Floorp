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

import glob, logging, os, subprocess, sys
import re

__all__ = [
  "addCommonOptions",
  "checkForCrashes",
  "dumpLeakLog",
  "processLeakLog",
  ]

log = logging.getLogger()

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
                    help = "absolute path to directory containing breakpad symbols")

def checkForCrashes(dumpDir, symbolsPath, testName=None):
  stackwalkPath = os.environ.get('MINIDUMP_STACKWALK', None)
  # try to get the caller's filename if no test name is given
  if testName is None:
    try:
      testName = os.path.basename(sys._getframe(1).f_code.co_filename)
    except:
      testName = "unknown"

  foundCrash = False
  dumps = glob.glob(os.path.join(dumpDir, '*.dmp'))
  for d in dumps:
    log.info("TEST-UNEXPECTED-FAIL | %s | application crashed (minidump found)", testName)
    if symbolsPath and stackwalkPath:
      nullfd = open(os.devnull, 'w')
      # eat minidump_stackwalk errors
      subprocess.call([stackwalkPath, d, symbolsPath], stderr=nullfd)
      nullfd.close()
    os.remove(d)
    extra = os.path.splitext(d)[0] + ".extra"
    if os.path.exists(extra):
      os.remove(extra)
    foundCrash = True

  return foundCrash

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

def processLeakLog(leakLogFile, leakThreshold = 0):
  """Process the leak log, parsing it.

  Use this function if you want an additional PASS/FAIL summary.
  It must be used with the |XPCOM_MEM_BLOAT_LOG| environment variable.
  """

  if not os.path.exists(leakLogFile):
    log.info("WARNING refcount logging is off, so leaks can't be detected!")
    return

  #                  Per-Inst  Leaked      Total  Rem ...
  #   0 TOTAL              17     192  419115886    2 ...
  # 833 nsTimerImpl        60     120      24726    2 ...
  lineRe = re.compile(r"^\s*\d+\s+(?P<name>\S+)\s+"
                      r"(?P<size>-?\d+)\s+(?P<bytesLeaked>-?\d+)\s+"
                      r"-?\d+\s+(?P<numLeaked>-?\d+)")

  leaks = open(leakLogFile, "r")
  for line in leaks:
    matches = lineRe.match(line)
    if (matches and
        int(matches.group("numLeaked")) == 0 and
        matches.group("name") != "TOTAL"):
      continue
    log.info(line.rstrip())
  leaks.close()

  leaks = open(leakLogFile, "r")
  seenTotal = False
  prefix = "TEST-PASS"
  for line in leaks:
    matches = lineRe.match(line)
    if not matches:
      continue
    name = matches.group("name")
    size = int(matches.group("size"))
    bytesLeaked = int(matches.group("bytesLeaked"))
    numLeaked = int(matches.group("numLeaked"))
    if size < 0 or bytesLeaked < 0 or numLeaked < 0:
      log.info("TEST-UNEXPECTED-FAIL | runtests-leaks | negative leaks caught!")
      if name == "TOTAL":
        seenTotal = True
    elif name == "TOTAL":
      seenTotal = True
      # Check for leaks.
      if bytesLeaked < 0 or bytesLeaked > leakThreshold:
        prefix = "TEST-UNEXPECTED-FAIL"
        leakLog = "TEST-UNEXPECTED-FAIL | runtests-leaks | leaked" \
                  " %d bytes during test execution" % bytesLeaked
      elif bytesLeaked > 0:
        leakLog = "TEST-PASS | runtests-leaks | WARNING leaked" \
                  " %d bytes during test execution" % bytesLeaked
      else:
        leakLog = "TEST-PASS | runtests-leaks | no leaks detected!"
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
        log.info("%(prefix)s | runtests-leaks | leaked %(numLeaked)d %(instance)s of %(name)s "
                 "with size %(size)s bytes%(rest)s" %
                 { "prefix": prefix,
                   "numLeaked": numLeaked,
                   "instance": instance,
                   "name": name,
                   "size": matches.group("size"),
                   "rest": rest })
  if not seenTotal:
    log.info("TEST-UNEXPECTED-FAIL | runtests-leaks | missing output line for total leaks!")
  leaks.close()
