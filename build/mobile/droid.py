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
# The Original Code is Test Automation Framework.
#
# The Initial Developer of the Original Code is
# Mozilla foundation
#
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Joel Maher <joel.maher@gmail.com> (Original Developer)
#   William Lachance <wlachance@mozilla.com>
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

from devicemanagerADB import DeviceManagerADB
from devicemanagerSUT import DeviceManagerSUT
import StringIO

class DroidMixin(object):
  """Mixin to extend DeviceManager with Android-specific functionality"""

  def launchApplication(self, app, activity="App",
                        intent="android.intent.action.VIEW", env=None,
                        url=None, extra_args=None):
    """
    Launches an Android application
    returns:
    success: True
    failure: False
    """
    # only one instance of an application may be running at once
    if self.processExist(app):
      return False

    acmd = [ "am", "start", "-W", "-n", "%s/.%s" % (app, activity)]

    if intent:
      acmd.extend(["-a", intent])

    if extra_args:
      acmd.extend(["--es", "args", " ".join(extra_args)])

    if env:
      envCnt = 0
      # env is expected to be a dict of environment variables
      for envkey, envval in env.iteritems():
        acmd.extend(["--es", "env" + str(envCnt), envkey + "=" + envval])
        envCnt += 1

    if url:
      acmd.extend(["-d", ''.join(["'", url, "'"])])

    # shell output not that interesting and debugging logs should already
    # show what's going on here... so just create an empty memory buffer
    # and ignore
    shellOutput = StringIO.StringIO()
    if self.shell(acmd, shellOutput) == 0:
      return True

    return False

class DroidADB(DeviceManagerADB, DroidMixin):
  pass

class DroidSUT(DeviceManagerSUT, DroidMixin):
  pass
