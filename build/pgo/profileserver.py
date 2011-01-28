#literal #!/usr/bin/python
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
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert Sayre <sayrer@gmail.com>
#   Jeff Walden <jwalden+bmo@mit.edu>
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

import SimpleHTTPServer
import SocketServer
import socket
import threading
import os
import sys
import shutil
from datetime import datetime
from automation import Automation
from automationutils import getDebuggerInfo, addCommonOptions

PORT = 8888
SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))
PROFILE_DIRECTORY = os.path.abspath(os.path.join(SCRIPT_DIR, "./pgoprofile"))
MOZ_JAR_LOG_DIR = os.path.abspath(os.path.join(os.path.join(os.getenv("OBJDIR"), "dist"), "jarlog"))
os.chdir(SCRIPT_DIR)

class EasyServer(SocketServer.TCPServer):
  allow_reuse_address = True

if __name__ == '__main__':
  from optparse import OptionParser
  automation = Automation()

  parser = OptionParser()
  addCommonOptions(parser)

  options, args = parser.parse_args()

  debuggerInfo = getDebuggerInfo(".", options.debugger, options.debuggerArgs,
          options.debuggerInteractive)

  httpd = EasyServer(("", PORT), SimpleHTTPServer.SimpleHTTPRequestHandler)
  t = threading.Thread(target=httpd.serve_forever)
  t.setDaemon(True) # don't hang on exit
  t.start()
  
  automation.setServerInfo("localhost", PORT)
  automation.initializeProfile(PROFILE_DIRECTORY)
  browserEnv = automation.environment()
  browserEnv["XPCOM_DEBUG_BREAK"] = "warn"
  browserEnv["MOZ_JAR_LOG_DIR"] = MOZ_JAR_LOG_DIR

  url = "http://localhost:%d/index.html" % PORT
  appPath = os.path.join(SCRIPT_DIR, automation.DEFAULT_APP)
  status = automation.runApp(url, browserEnv, appPath, PROFILE_DIRECTORY, {},
                             debuggerInfo=debuggerInfo,
                             # the profiling HTML doesn't output anything,
                             # so let's just run this without a timeout
                             timeout = None)
  sys.exit(status)
