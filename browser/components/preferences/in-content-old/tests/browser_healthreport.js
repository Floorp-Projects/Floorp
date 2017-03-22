/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

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
  runPaneTest(testBasic);
}

function testBasic(win, doc) {
  is(Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED), true,
     "Health Report upload enabled on app first run.");

  let checkbox = doc.getElementById("submitHealthReportBox");
  ok(checkbox);
  is(checkbox.checked, true, "Health Report checkbox is checked on app first run.");

  checkbox.checked = false;
  checkbox.doCommand();
  is(Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED), false,
     "Unchecking checkbox opts out of FHR upload.");

  checkbox.checked = true;
  checkbox.doCommand();
  is(Services.prefs.getBoolPref(FHR_UPLOAD_ENABLED), true,
     "Checking checkbox allows FHR upload.");

  win.close();
  Services.prefs.lockPref(FHR_UPLOAD_ENABLED);
  runPaneTest(testUploadDisabled);
}

function testUploadDisabled(win, doc) {
  ok(Services.prefs.prefIsLocked(FHR_UPLOAD_ENABLED), "Upload enabled flag is locked.");
  let checkbox = doc.getElementById("submitHealthReportBox");
  is(checkbox.getAttribute("disabled"), "true", "Checkbox is disabled if upload flag is locked.");
  Services.prefs.unlockPref(FHR_UPLOAD_ENABLED);

  win.close();
  finish();
}

function resetPreferences() {
  Services.prefs.clearUserPref(FHR_UPLOAD_ENABLED);
}

