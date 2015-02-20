/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function runPaneTest(fn) {
  open_preferences((win) => {
    let doc = win.document;
    win.gotoPref("paneAdvanced");
    let advancedPrefs = doc.getElementById("advancedPrefs");
    let tab = doc.getElementById("dataChoicesTab");
    advancedPrefs.selectedTab = tab;
    fn(win, doc);
  });
}

function test() {
  waitForExplicitFinish();
  resetPreferences();
  registerCleanupFunction(resetPreferences);
  runPaneTest(testTelemetryState);
}

function testTelemetryState(win, doc) {
  let fhrCheckbox = doc.getElementById("submitHealthReportBox");
  Assert.ok(fhrCheckbox.checked, "Health Report checkbox is checked on app first run.");

  let telmetryCheckbox = doc.getElementById("submitTelemetryBox");
  Assert.ok(!telmetryCheckbox.disabled,
            "Telemetry checkbox must be enabled if FHR is checked.");

  // Uncheck the FHR checkbox and make sure that Telemetry checkbox gets disabled.
  fhrCheckbox.click();

  Assert.ok(telmetryCheckbox.disabled,
            "Telemetry checkbox must be disabled if FHR is unchecked.");

  win.close();
  finish();
}

function resetPreferences() {
  Services.prefs.clearUserPref("datareporting.healthreport.uploadEnabled");
}

