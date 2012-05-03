# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

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
