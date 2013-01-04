/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  resetPreferences();
  registerCleanupFunction(resetPreferences);
  open_preferences(runTest);
}

function runTest(win) {
  let doc = win.document;

  win.gotoPref("paneAdvanced");
  let advancedPrefs = doc.getElementById("advancedPrefs");
  let dataChoicesTab = doc.getElementById("dataChoicesTab");
  advancedPrefs.selectedTab = dataChoicesTab;

  let checkbox = doc.getElementById("submitHealthReportBox");
  ok(checkbox);
  is(checkbox.checked, false, "Health Report checkbox is unchecked on app first run.");

  let reporter = Components.classes["@mozilla.org/healthreport/service;1"]
                                   .getService(Components.interfaces.nsISupports)
                                   .wrappedJSObject
                                   .reporter;
  ok(reporter);
  is(reporter.dataSubmissionPolicyAccepted, false, "Data submission policy not accepted.");

  checkbox.checked = true;
  checkbox.doCommand();
  is(reporter.dataSubmissionPolicyAccepted, true, "Checking checkbox accepts data submission policy.");
  checkbox.checked = false;
  checkbox.doCommand();
  is(reporter.dataSubmissionPolicyAccepted, false, "Unchecking checkbox opts out of data submission.");

  win.close();
  finish();
}

function resetPreferences() {
  Services.prefs.clearUserPref("healthreport.policy.dataSubmissionPolicyAccepted");
}
