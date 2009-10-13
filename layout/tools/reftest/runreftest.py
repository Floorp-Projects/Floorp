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
# The Initial Developer of the Original Code is
# Mozilla Foundation.
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
# ***** END LICENSE BLOCK *****

"""
Runs the reftest test harness.
"""

import sys, shutil, os, os.path
SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))
sys.path.append(SCRIPT_DIRECTORY)
import automation
from automationutils import *
from optparse import OptionParser
from tempfile import mkdtemp

oldcwd = os.getcwd()
os.chdir(SCRIPT_DIRECTORY)

def getFullPath(path):
  "Get an absolute path relative to oldcwd."
  return os.path.normpath(os.path.join(oldcwd, os.path.expanduser(path)))

def createReftestProfile(options, profileDir):
  "Sets up a profile for reftest."

  # Set preferences.
  prefsFile = open(os.path.join(profileDir, "user.js"), "w")
  prefsFile.write("""user_pref("browser.dom.window.dump.enabled", true);
""")
  prefsFile.write('user_pref("reftest.timeout", %d);' % (options.timeout * 1000))
  prefsFile.write('user_pref("ui.caretBlinkTime", -1);')
  # no slow script dialogs
  prefsFile.write('user_pref("dom.max_script_run_time", 0);')
  prefsFile.write('user_pref("dom.max_chrome_script_run_time", 0);')
  prefsFile.close()

  # install the reftest extension bits into the profile
  profileExtensionsPath = os.path.join(profileDir, "extensions")
  os.mkdir(profileExtensionsPath)
  reftestExtensionPath = os.path.join(SCRIPT_DIRECTORY, "reftest")
  extFile = open(os.path.join(profileExtensionsPath, "reftest@mozilla.org"), "w")
  extFile.write(reftestExtensionPath)
  extFile.close()

def main():
  parser = OptionParser()

  # we want to pass down everything from automation.__all__
  addCommonOptions(parser, defaults=dict(zip(automation.__all__, [getattr(automation, x) for x in automation.__all__])))
  parser.add_option("--appname",
                    action = "store", type = "string", dest = "app",
                    default = os.path.join(SCRIPT_DIRECTORY, automation.DEFAULT_APP),
                    help = "absolute path to application, overriding default")
  parser.add_option("--extra-profile-file",
                    action = "append", dest = "extraProfileFiles",
                    default = [],
                    help = "copy specified files/dirs to testing profile")
  parser.add_option("--timeout",              
                    action = "store", dest = "timeout", type = "int", 
                    default = 5 * 60, # 5 minutes per bug 479518
                    help = "reftest will timeout in specified number of seconds. [default %default s].")
  parser.add_option("--leak-threshold",
                    action = "store", type = "int", dest = "leakThreshold",
                    default = 0,
                    help = "fail if the number of bytes leaked through "
                           "refcounted objects (or bytes in classes with "
                           "MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) is greater "
                           "than the given number")
  parser.add_option("--utility-path",
                    action = "store", type = "string", dest = "utilityPath",
                    default = automation.DIST_BIN,
                    help = "absolute path to directory containing utility "
                           "programs (xpcshell, ssltunnel, certutil)")

  options, args = parser.parse_args()

  if len(args) != 1:
    print >>sys.stderr, "No reftest.list specified."
    sys.exit(1)

  options.app = getFullPath(options.app)
  if not os.path.exists(options.app):
    print """Error: Path %(app)s doesn't exist.
Are you executing $objdir/_tests/reftest/runreftest.py?""" \
        % {"app": options.app}
    sys.exit(1)

  if options.xrePath is None:
    options.xrePath = os.path.dirname(options.app)
  else:
    # allow relative paths
    options.xrePath = getFullPath(options.xrePath)

  if options.symbolsPath:
    options.symbolsPath = getFullPath(options.symbolsPath)
  options.utilityPath = getFullPath(options.utilityPath)

  debuggerInfo = getDebuggerInfo(oldcwd, options.debugger, options.debuggerArgs,
     options.debuggerInteractive);

  profileDir = None
  try:
    profileDir = mkdtemp()
    createReftestProfile(options, profileDir)
    copyExtraFilesToProfile(options, profileDir)

    # browser environment
    browserEnv = automation.environment(xrePath = options.xrePath)
    browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

    # Enable leaks detection to its own log file.
    leakLogFile = os.path.join(profileDir, "runreftest_leaks.log")
    browserEnv["XPCOM_MEM_BLOAT_LOG"] = leakLogFile

    # run once with -silent to let the extension manager do its thing
    # and then exit the app
    automation.log.info("REFTEST INFO | runreftest.py | Performing extension manager registration: start.\n")
    # Don't care about this |status|: |runApp()| reporting it should be enough.
    status = automation.runApp(None, browserEnv, options.app, profileDir,
                               ["-silent"],
                               utilityPath = options.utilityPath,
                               xrePath=options.xrePath,
                               symbolsPath=options.symbolsPath)
    # We don't care to call |processLeakLog()| for this step.
    automation.log.info("\nREFTEST INFO | runreftest.py | Performing extension manager registration: end.")

    # Remove the leak detection file so it can't "leak" to the tests run.
    # The file is not there if leak logging was not enabled in the application build.
    if os.path.exists(leakLogFile):
      os.remove(leakLogFile)

    # then again to actually run reftest
    automation.log.info("REFTEST INFO | runreftest.py | Running tests: start.\n")
    reftestlist = getFullPath(args[0])
    status = automation.runApp(None, browserEnv, options.app, profileDir,
                               ["-reftest", reftestlist],
                               utilityPath = options.utilityPath,
                               xrePath=options.xrePath,
                               debuggerInfo=debuggerInfo,
                               symbolsPath=options.symbolsPath,
                               # give the JS harness 30 seconds to deal
                               # with its own timeouts
                               timeout=options.timeout + 30.0)
    processLeakLog(leakLogFile, options.leakThreshold)
    automation.log.info("\nREFTEST INFO | runreftest.py | Running tests: end.")
  finally:
    if profileDir:
      shutil.rmtree(profileDir)
  sys.exit(status)

def copyExtraFilesToProfile(options, profileDir):
  "Copy extra files or dirs specified on the command line to the testing profile."
  for f in options.extraProfileFiles:
    abspath = getFullPath(f)
    dest = os.path.join(profileDir, os.path.basename(abspath))
    if os.path.isdir(abspath):
      shutil.copytree(abspath, dest)
    else:
      shutil.copy(abspath, dest)

if __name__ == "__main__":
  main()
