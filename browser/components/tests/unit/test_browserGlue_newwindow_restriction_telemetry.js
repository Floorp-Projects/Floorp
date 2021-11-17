/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"].getService(
  Ci.nsIObserver
);

Cu.importGlobalProperties(["Glean"]);

const RESTRICTION_PREF = "browser.link.open_newwindow.restriction";

function collectTelemetry() {
  gBrowserGlue.observe(
    null,
    "browser-glue-test",
    "new-window-restriction-telemetry"
  );
}

add_task(async function() {
  const FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG();

  Services.prefs.setIntPref(RESTRICTION_PREF, 0);
  collectTelemetry();
  Assert.equal(Glean.browserLink.openNewwindowRestriction.testGetValue(), 0);

  Services.prefs.setIntPref(RESTRICTION_PREF, 1);
  collectTelemetry();
  Assert.equal(Glean.browserLink.openNewwindowRestriction.testGetValue(), 1);

  Services.prefs.setIntPref(RESTRICTION_PREF, 2);
  collectTelemetry();
  Assert.equal(Glean.browserLink.openNewwindowRestriction.testGetValue(), 2);

  // Test 0 again to verify that 0 above wasn't garbage data, and also as a
  // preparetion for the next default pref test.
  Services.prefs.setIntPref(RESTRICTION_PREF, 0);
  collectTelemetry();
  Assert.equal(Glean.browserLink.openNewwindowRestriction.testGetValue(), 0);

  Services.prefs.clearUserPref(RESTRICTION_PREF);
  collectTelemetry();
  Assert.equal(Glean.browserLink.openNewwindowRestriction.testGetValue(), 2);
});

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(RESTRICTION_PREF);
});
