/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;

let TargetFactory = (function() {
  let tempScope = {};
  Components.utils.import("resource:///modules/devtools/Target.jsm", tempScope);
  return tempScope.TargetFactory;
})();

// Clear preferences that may be set during the course of tests.
function clearUserPrefs()
{
  Services.prefs.clearUserPref("devtools.inspector.htmlPanelOpen");
  Services.prefs.clearUserPref("devtools.inspector.sidebarOpen");
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
}

registerCleanupFunction(clearUserPrefs);
