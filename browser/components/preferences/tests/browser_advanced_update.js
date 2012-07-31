/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  resetPreferences();

  registerCleanupFunction(resetPreferences);

  function observer(win, topic, data) {
    Services.obs.removeObserver(observer, "advanced-pane-loaded");
    runTest(win);
  }
  Services.obs.addObserver(observer, "advanced-pane-loaded", false);

  Services.prefs.setBoolPref("browser.search.update", false);
  openDialog("chrome://browser/content/preferences/preferences.xul", "Preferences",
             "chrome,titlebar,toolbar,centerscreen,dialog=no", "paneAdvanced");
}

function runTest(win) {
  let doc = win.document;
  let enableSearchUpdate = doc.getElementById("enableSearchUpdate");

  // Ensure that the update pref dialog reflects the actual pref value.
  ok(!enableSearchUpdate.checked, "Ensure search updates are disabled");
  Services.prefs.setBoolPref("browser.search.update", true);
  ok(enableSearchUpdate.checked, "Ensure search updates are enabled");

  win.close();
  finish();
}

function resetPreferences() {
  Services.prefs.clearUserPref("browser.search.update");
}
