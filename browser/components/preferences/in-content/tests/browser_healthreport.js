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

  let policy = Components.classes["@mozilla.org/datareporting/service;1"]
                                 .getService(Components.interfaces.nsISupports)
                                 .wrappedJSObject
                                 .policy;
  ok(policy);
  is(policy.dataSubmissionPolicyAccepted, false, "Data submission policy not accepted.");
  is(policy.healthReportUploadEnabled, true, "Health Report upload enabled on app first run.");

  let checkbox = doc.getElementById("submitHealthReportBox");
  ok(checkbox);
  is(checkbox.checked, true, "Health Report checkbox is checked on app first run.");

  checkbox.checked = false;
  checkbox.doCommand();
  is(policy.healthReportUploadEnabled, false, "Unchecking checkbox opts out of FHR upload.");

  checkbox.checked = true;
  checkbox.doCommand();
  is(policy.healthReportUploadEnabled, true, "Checking checkbox allows FHR upload.");

  win.close();
  finish();
}

function resetPreferences() {
  Services.prefs.clearUserPref("datareporting.healthreport.uploadEnabled");
}

