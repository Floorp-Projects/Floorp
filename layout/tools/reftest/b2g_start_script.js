/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function setDefaultPrefs() {
    // This code sets the preferences for extension-based reftest.
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService);
    var branch = prefs.getDefaultBranch("");

#include reftest-preferences.js
}

function setPermissions() {
  if (__marionetteParams.length < 2) {
    return;
  }

  let serverAddr = __marionetteParams[0];
  let serverPort = __marionetteParams[1];
  let perms = Cc["@mozilla.org/permissionmanager;1"]
              .getService(Ci.nsIPermissionManager);
  let ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  let uri = ioService.newURI("http://" + serverAddr + ":" + serverPort, null, null);
  perms.add(uri, "allowXULXBL", Ci.nsIPermissionManager.ALLOW_ACTION);
}

let cm = Cc["@mozilla.org/categorymanager;1"]
           .getService(Components.interfaces.nsICategoryManager);

// Disable update timers that cause b2g failures.
if (cm) {
  cm.deleteCategoryEntry("update-timer", "WebappsUpdateTimer", false);
  cm.deleteCategoryEntry("update-timer", "nsUpdateService", false);
}

// Load into any existing windows
let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
            .getService(Ci.nsIWindowMediator);
let win = wm.getMostRecentWindow('');

// Set preferences and permissions
setDefaultPrefs();
setPermissions();

// Loading this into the global namespace causes intermittent failures.
// See bug 882888 for more details.
let reftest = {};
Cu.import("chrome://reftest/content/reftest.jsm", reftest);

// Start the reftests
reftest.OnRefTestLoad(win);
