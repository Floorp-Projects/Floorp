"use strict";

function switchToCustomHistoryMode(doc) {
  // Select the last item in the menulist.
  let menulist = doc.getElementById("historyMode");
  menulist.focus();
  EventUtils.sendKey("UP");
}

function testPrefStateMatchesLockedState() {
  let win = gBrowser.contentWindow;
  let doc = win.document;
  switchToCustomHistoryMode(doc);

  let checkbox = doc.getElementById("alwaysClear");
  let preference = win.Preferences.get("privacy.sanitize.sanitizeOnShutdown");
  is(
    checkbox.disabled,
    preference.locked,
    "Always Clear checkbox should be enabled when preference is not locked."
  );

  Services.prefs.clearUserPref("privacy.history.custom");
  gBrowser.removeCurrentTab();
}

add_task(function setup() {
  registerCleanupFunction(function resetPreferences() {
    Services.prefs.unlockPref("privacy.sanitize.sanitizeOnShutdown");
    Services.prefs.clearUserPref("privacy.history.custom");
  });
});

add_task(async function test_preference_enabled_when_unlocked() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  testPrefStateMatchesLockedState();
});

add_task(async function test_preference_disabled_when_locked() {
  Services.prefs.lockPref("privacy.sanitize.sanitizeOnShutdown");
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  testPrefStateMatchesLockedState();
});
