/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";

function runPaneTest(fn) {
  function observer(win, topic, data) {
    Services.obs.removeObserver(observer, "advanced-pane-loaded");

    let policy = Components.classes["@mozilla.org/datareporting/service;1"]
                                   .getService(Components.interfaces.nsISupports)
                                   .wrappedJSObject
                                   .policy;
    Assert.ok(policy, "Policy object defined.");

    resetPreferences();

    fn(win);
  }

  Services.obs.addObserver(observer, "advanced-pane-loaded", false);
  openDialog("chrome://browser/content/preferences/preferences.xul", "Preferences",
             "chrome,titlebar,toolbar,centerscreen,dialog=no", "paneAdvanced");
}

function test() {
  waitForExplicitFinish();
  resetPreferences();
  registerCleanupFunction(resetPreferences);

  runPaneTest(testTelemetryState);
}

function testTelemetryState(win) {
  let doc = win.document;

  let fhrCheckbox = doc.getElementById("submitHealthReportBox");
  Assert.ok(fhrCheckbox.checked, "Health Report checkbox is checked on app first run.");

  let telmetryCheckbox = doc.getElementById("submitTelemetryBox");
  Assert.ok(!telmetryCheckbox.disabled,
            "Telemetry checkbox must be enabled if FHR is checked.");
  Assert.ok(Services.prefs.getBoolPref(PREF_TELEMETRY_ENABLED),
            "Telemetry must be enabled if the checkbox is ticked.");

  // Uncheck the FHR checkbox and make sure that Telemetry checkbox gets disabled.
  fhrCheckbox.click();

  Assert.ok(telmetryCheckbox.disabled,
            "Telemetry checkbox must be disabled if FHR is unchecked.");
  Assert.ok(!Services.prefs.getBoolPref(PREF_TELEMETRY_ENABLED),
            "Telemetry must be disabled if the checkbox is unticked.");

  win.close();
  finish();
}

function resetPreferences() {
  let service = Cc["@mozilla.org/datareporting/service;1"]
                  .getService(Ci.nsISupports)
                  .wrappedJSObject;
  service.policy._prefs.resetBranch("datareporting.policy.");
  service.policy.dataSubmissionPolicyBypassNotification = true;
  Services.prefs.clearUserPref(PREF_TELEMETRY_ENABLED);
}

