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
# The Original Code is Mozilla Firebird about dialog.
#
# The Initial Developer of the Original Code is
# Blake Ross (blaker@netscape.com).
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ehsan Akhgari <ehsan.akhgari@gmail.com>
#   Margaret Leibovic <margaret.leibovic@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** -->

// Services = object with smart getters for common XPCOM services
Components.utils.import("resource://gre/modules/Services.jsm");

function init(aEvent)
{
  if (aEvent.target != document)
    return;

  try {
    var distroId = Services.prefs.getCharPref("distribution.id");
    if (distroId) {
      var distroVersion = Services.prefs.getCharPref("distribution.version");
      var distroAbout = Services.prefs.getComplexValue("distribution.about",
        Components.interfaces.nsISupportsString);

      var distroField = document.getElementById("distribution");
      distroField.value = distroAbout;
      distroField.style.display = "block";

      var distroIdField = document.getElementById("distributionId");
      distroIdField.value = distroId + " - " + distroVersion;
      distroIdField.style.display = "block";
    }
  }
  catch (e) {
    // Pref is unset
  }

#ifdef MOZ_UPDATER
  initUpdates();
#endif

#ifdef XP_MACOSX
  // it may not be sized at this point, and we need its width to calculate its position
  window.sizeToContent();
  window.moveTo((screen.availWidth / 2) - (window.outerWidth / 2), screen.availHeight / 5);
#endif
}

#ifdef MOZ_UPDATER
/**
 * Creates "Last Updated" message and sets up "Check for Updates..." button.
 */
function initUpdates()
{
  var um = Components.classes["@mozilla.org/updates/update-manager;1"].
           getService(Components.interfaces.nsIUpdateManager);
  var browserBundle = Services.strings.
                      createBundle("chrome://browser/locale/browser.properties");

  if (um.updateCount) {
    let buildID = um.getUpdateAt(0).buildID;
    let released = browserBundle.formatStringFromName("aboutdialog.released", 
                                                      [buildID.substring(0, 4), 
                                                       buildID.substring(4, 6), 
                                                       buildID.substring(6, 8)], 3);
    document.getElementById("released").setAttribute("value", released);
  }

  var checkForUpdates = document.getElementById("checkForUpdatesButton");
  setupCheckForUpdates(checkForUpdates, browserBundle);
}
#endif
