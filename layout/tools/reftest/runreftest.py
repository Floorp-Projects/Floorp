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

from automation import Automation
from automationutils import *
from optparse import OptionParser
from tempfile import mkdtemp

class RefTest(object):

  oldcwd = os.getcwd()

  def __init__(self, automation):
    self.automation = automation

  def getFullPath(self, path):
    "Get an absolute path relative to self.oldcwd."
    return os.path.normpath(os.path.join(self.oldcwd, os.path.expanduser(path)))

  def getManifestPath(self, path):
    "Get the path of the manifest, and for remote testing this function is subclassed to point to remote manifest"
    return self.getFullPath(path)

  def createReftestProfile(self, options, profileDir):
    "Sets up a profile for reftest."

    self.automation.setupPermissionsDatabase(profileDir,
      {'allowXULXBL': [('localhost', True), ('<file>', True)]})

    # Set preferences.
    prefsFile = open(os.path.join(profileDir, "user.js"), "w")
    prefsFile.write("""user_pref("browser.dom.window.dump.enabled", true);
    """)
    prefsFile.write('user_pref("reftest.timeout", %d);\n' % (options.timeout * 1000))
    prefsFile.write('user_pref("ui.caretBlinkTime", -1);\n')

    if options.totalChunks != None:
      prefsFile.write('user_pref("reftest.totalChunks", %d);\n' % options.totalChunks)
    if options.thisChunk != None:
      prefsFile.write('user_pref("reftest.thisChunk", %d);\n' % options.thisChunk)
    if options.logFile != None:
      prefsFile.write('user_pref("reftest.logFile", "%s");\n' % options.logFile)

    for v in options.extraPrefs:
      thispref = v.split("=")
      if len(thispref) < 2:
        print "Error: syntax error in --setpref=" + v
        sys.exit(1)
      part = 'user_pref("%s", %s);\n' % (thispref[0], thispref[1])
      prefsFile.write(part)
    # no slow script dialogs
    prefsFile.write('user_pref("dom.max_script_run_time", 0);')
    prefsFile.write('user_pref("dom.max_chrome_script_run_time", 0);')
    prefsFile.close()

    # install the reftest extension bits into the profile
    self.automation.installExtension(os.path.join(SCRIPT_DIRECTORY, "reftest"),
                                                  profileDir,
                                                  "reftest@mozilla.org")


  def registerExtension(self, browserEnv, options, profileDir, extraArgs = ['-silent']):
    # run once with -silent to let the extension manager do its thing
    # and then exit the app
    self.automation.log.info("REFTEST INFO | runreftest.py | Performing extension manager registration: start.\n")
    # Don't care about this |status|: |runApp()| reporting it should be enough.
    status = self.automation.runApp(None, browserEnv, options.app, profileDir,
                                 extraArgs,
                                 utilityPath = options.utilityPath,
                                 xrePath=options.xrePath,
                                 symbolsPath=options.symbolsPath)
    # We don't care to call |processLeakLog()| for this step.
    self.automation.log.info("\nREFTEST INFO | runreftest.py | Performing extension manager registration: end.")

    # Remove the leak detection file so it can't "leak" to the tests run.
    # The file is not there if leak logging was not enabled in the application build.
    if os.path.exists(self.leakLogFile):
      os.remove(self.leakLogFile)

  def buildBrowserEnv(self, options, profileDir):
    browserEnv = self.automation.environment(xrePath = options.xrePath)
    browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

    # Enable leaks detection to its own log file.
    self.leakLogFile = os.path.join(profileDir, "runreftest_leaks.log")
    browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leakLogFile
    return browserEnv

  def cleanup(self, profileDir):
    if profileDir:
      shutil.rmtree(profileDir)

  def runTests(self, manifest, options):
    debuggerInfo = getDebuggerInfo(self.oldcwd, options.debugger, options.debuggerArgs,
        options.debuggerInteractive);

    profileDir = None
    try:
      profileDir = mkdtemp()
      self.createReftestProfile(options, profileDir)
      self.copyExtraFilesToProfile(options, profileDir)

      # browser environment
      browserEnv = self.buildBrowserEnv(options, profileDir)

      self.registerExtension(browserEnv, options, profileDir)

      # then again to actually run reftest
      self.automation.log.info("REFTEST INFO | runreftest.py | Running tests: start.\n")
      reftestlist = self.getManifestPath(manifest)
      status = self.automation.runApp(None, browserEnv, options.app, profileDir,
                                 ["-reftest", reftestlist],
                                 utilityPath = options.utilityPath,
                                 xrePath=options.xrePath,
                                 debuggerInfo=debuggerInfo,
                                 symbolsPath=options.symbolsPath,
                                 # give the JS harness 30 seconds to deal
                                 # with its own timeouts
                                 timeout=options.timeout + 30.0)
      processLeakLog(self.leakLogFile, options.leakThreshold)
      self.automation.log.info("\nREFTEST INFO | runreftest.py | Running tests: end.")
    finally:
      self.cleanup(profileDir)
    return status

  def copyExtraFilesToProfile(self, options, profileDir):
    "Copy extra files or dirs specified on the command line to the testing profile."
    for f in options.extraProfileFiles:
      abspath = self.getFullPath(f)
      dest = os.path.join(profileDir, os.path.basename(abspath))
      if os.path.isdir(abspath):
        shutil.copytree(abspath, dest)
      else:
        shutil.copy(abspath, dest)


class ReftestOptions(OptionParser):

  def __init__(self, automation):
    self._automation = automation
    OptionParser.__init__(self)
    defaults = {}

    # we want to pass down everything from automation.__all__
    addCommonOptions(self, 
                     defaults=dict(zip(self._automation.__all__, 
                            [getattr(self._automation, x) for x in self._automation.__all__])))
    self._automation.addCommonOptions(self)
    self.add_option("--appname",
                    action = "store", type = "string", dest = "app",
                    default = os.path.join(SCRIPT_DIRECTORY, automation.DEFAULT_APP),
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
                    action = "store", type = "int", dest = "leakThreshold",
                    default = 0,
                    help = "fail if the number of bytes leaked through "
                           "refcounted objects (or bytes in classes with "
                           "MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) is greater "
                           "than the given number")
    self.add_option("--utility-path",
                    action = "store", type = "string", dest = "utilityPath",
                    default = self._automation.DIST_BIN,
                    help = "absolute path to directory containing utility "
                           "programs (xpcshell, ssltunnel, certutil)")
    defaults["utilityPath"] = self._automation.DIST_BIN

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

    self.set_defaults(**defaults)

def main():
  automation = Automation()
  parser = ReftestOptions(automation)
  reftest = RefTest(automation)

  options, args = parser.parse_args()
  if len(args) != 1:
    print >>sys.stderr, "No reftest.list specified."
    sys.exit(1)

  options.app = reftest.getFullPath(options.app)
  if not os.path.exists(options.app):
    print """Error: Path %(app)s doesn't exist.
Are you executing $objdir/_tests/reftest/runreftest.py?""" \
            % {"app": options.app}
    sys.exit(1)

  if options.xrePath is None:
    options.xrePath = os.path.dirname(options.app)
  else:
    # allow relative paths
    options.xrePath = reftest.getFullPath(options.xrePath)

  if options.symbolsPath and not isURL(options.symbolsPath):
    options.symbolsPath = reftest.getFullPath(options.symbolsPath)
  options.utilityPath = reftest.getFullPath(options.utilityPath)

  if options.totalChunks is not None and options.thisChunk is None:
    print "thisChunk must be specified when totalChunks is specified"
    sys.exit(1)

  if options.totalChunks:
    if not 1 <= options.thisChunk <= options.totalChunks:
      print "thisChunk must be between 1 and totalChunks"
      sys.exit(1)
  
  if options.logFile:
    options.logFile = reftest.getFullPath(options.logFile)

  sys.exit(reftest.runTests(args[0], options))
  
if __name__ == "__main__":
  main()
