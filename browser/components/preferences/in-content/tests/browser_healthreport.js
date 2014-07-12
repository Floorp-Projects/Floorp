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

    let policy = Components.classes["@mozilla.org/datareporting/service;1"]
                                   .getService(Components.interfaces.nsISupports)
                                   .wrappedJSObject
                                   .policy;

    ok(policy, "Policy object is defined.");
    fn(win, doc, policy);
  });
}

function test() {
  waitForExplicitFinish();
  resetPreferences();
  registerCleanupFunction(resetPreferences);
  runPaneTest(testBasic);
}

function testBasic(win, doc, policy) {
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
  Services.prefs.lockPref("datareporting.healthreport.uploadEnabled");
  runPaneTest(testUploadDisabled);
}

function testUploadDisabled(win, doc, policy) {
  ok(policy.healthReportUploadLocked, "Upload enabled flag is locked.");
  let checkbox = doc.getElementById("submitHealthReportBox");
  is(checkbox.getAttribute("disabled"), "true", "Checkbox is disabled if upload flag is locked.");
  policy._healthReportPrefs.unlock("uploadEnabled");

  win.close();
  finish();
}

function resetPreferences() {
  Services.prefs.clearUserPref("datareporting.healthreport.uploadEnabled");
}

