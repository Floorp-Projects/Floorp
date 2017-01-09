/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;

function setPermissions() {
  if (__webDriverArguments.length < 2) {
    return;
  }

  let serverAddr = __webDriverArguments[0];
  let serverPort = __webDriverArguments[1];
  let perms = Cc["@mozilla.org/permissionmanager;1"]
              .getService(Ci.nsIPermissionManager);
  let ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  let uri = ioService.newURI("http://" + serverAddr + ":" + serverPort);
  perms.add(uri, "allowXULXBL", Ci.nsIPermissionManager.ALLOW_ACTION);
}

var cm = Cc["@mozilla.org/categorymanager;1"]
           .getService(Ci.nsICategoryManager);

// Disable update timers that cause b2g failures.
if (cm) {
  cm.deleteCategoryEntry("update-timer", "nsUpdateService", false);
}

// Load into any existing windows
var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
            .getService(Ci.nsIWindowMediator);
var win = wm.getMostRecentWindow('');

setPermissions();

// Loading this into the global namespace causes intermittent failures.
// See bug 882888 for more details.
var reftest = {};
Cu.import("chrome://reftest/content/reftest.jsm", reftest);

// Prevent display off during testing.
navigator.mozPower.screenEnabled = true;
var settingLock = navigator.mozSettings.createLock();
var settingResult = settingLock.set({
  'screen.timeout': 0
});
settingResult.onsuccess = function () {
  dump("Set screen.time to 0\n");
  // Start the reftests
  reftest.OnRefTestLoad(win);
}
settingResult.onerror = function () {
  dump("Change screen.time failed\n");
  // Start the reftests
  reftest.OnRefTestLoad(win);
}
