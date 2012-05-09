# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from devicemanagerADB import DeviceManagerADB
from devicemanagerSUT import DeviceManagerSUT
import StringIO

class DroidMixin(object):
  """Mixin to extend DeviceManager with Android-specific functionality"""

  def launchApplication(self, appName, activityName, intent, url=None,
                        extras=None):
    """
    Launches an Android application
    returns:
    success: True
    failure: False
    """
    # only one instance of an application may be running at once
    if self.processExist(appName):
      return False

    acmd = [ "am", "start", "-W", "-n", "%s/%s" % (appName, activityName)]

    if intent:
      acmd.extend(["-a", intent])

    if extras:
      for (key, val) in extras.iteritems():
        if type(val) is int:
          extraTypeParam = "--ei"
        elif type(val) is bool:
          extraTypeParam = "--ez"
        else:
          extraTypeParam = "--es"
        acmd.extend([extraTypeParam, str(key), str(val)])

    if url:
      acmd.extend(["-d", url])

    # shell output not that interesting and debugging logs should already
    # show what's going on here... so just create an empty memory buffer
    # and ignore
    shellOutput = StringIO.StringIO()
    if self.shell(acmd, shellOutput) == 0:
      return True

    return False

  def launchFennec(self, appName, intent="android.intent.action.VIEW",
                   mozEnv=None, extraArgs=None, url=None):
    """
    Convenience method to launch Fennec on Android with various debugging
    arguments
    WARNING: FIXME: This would go better in mozrunner. Please do not
    use this method if you are not comfortable with it going away sometime
    in the near future
    returns:
    success: True
    failure: False
    """
    extras = {}

    if mozEnv:
      # mozEnv is expected to be a dictionary of environment variables: Fennec
      # itself will set them when launched
      for (envCnt, (envkey, envval)) in enumerate(mozEnv.iteritems()):
        extras["env" + str(envCnt)] = envkey + "=" + envval

    # Additional command line arguments that fennec will read and use (e.g.
    # with a custom profile)
    if extraArgs:
      extras['args'] = " ".join(extraArgs)

    return self.launchApplication(appName, ".App", intent, url=url,
                                  extras=extras)

class DroidADB(DeviceManagerADB, DroidMixin):
  pass

class DroidSUT(DeviceManagerSUT, DroidMixin):
  pass
