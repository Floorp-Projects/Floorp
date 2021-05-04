/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This obj will be used in both tests
// First test makes sure accepting the preferences matches these values
// Second test makes sure the cancel dialog STILL matches these values
const syncPrefs = {
  "services.sync.engine.addons": false,
  "services.sync.engine.bookmarks": true,
  "services.sync.engine.history": true,
  "services.sync.engine.tabs": false,
  "services.sync.engine.prefs": false,
  "services.sync.engine.passwords": false,
  "services.sync.engine.addresses": false,
  "services.sync.engine.creditcards": false,
};

/**
 * We don't actually enable sync here, but we just check that the preferences are correct
 * when the callback gets hit (accepting/cancelling the dialog)
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1584132.
 */

add_task(async function testDialogAccept() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  // This will check if the callback was actually called during the test
  let callbackCalled = false;

  // Enabling all the sync UI is painful in tests, so we just open the dialog manually
  let syncWindow = await openAndLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml",
    null,
    {},
    () => {
      for (const [prefKey, prefValue] of Object.entries(syncPrefs)) {
        Assert.equal(
          Services.prefs.getBoolPref(prefKey),
          prefValue,
          `${prefValue} is expected value`
        );
      }
      callbackCalled = true;
    }
  );

  Assert.ok(syncWindow, "Choose what to sync window opened");
  let syncChooseDialog = syncWindow.document.getElementById(
    "syncChooseOptions"
  );
  let syncCheckboxes = syncChooseDialog.querySelectorAll(
    "checkbox[preference]"
  );

  // Adjust the checkbox values to the expectedValues in the list
  [...syncCheckboxes].forEach(checkbox => {
    if (syncPrefs[checkbox.getAttribute("preference")] !== checkbox.checked) {
      checkbox.click();
    }
  });

  syncChooseDialog.acceptDialog();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Assert.ok(callbackCalled, "Accept callback was called");
});

add_task(async function testDialogCancel() {
  const cancelSyncPrefs = {
    "services.sync.engine.addons": true,
    "services.sync.engine.bookmarks": false,
    "services.sync.engine.history": true,
    "services.sync.engine.tabs": true,
    "services.sync.engine.prefs": false,
    "services.sync.engine.passwords": true,
    "services.sync.engine.addresses": true,
    "services.sync.engine.creditcards": false,
  };

  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });

  // This will check if the callback was actually called during the test
  let callbackCalled = false;

  // Enabling all the sync UI is painful in tests, so we just open the dialog manually
  let syncWindow = await openAndLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/syncChooseWhatToSync.xhtml",
    null,
    {},
    () => {
      // We want to test against our previously accepted values in the last test
      for (const [prefKey, prefValue] of Object.entries(syncPrefs)) {
        Assert.equal(
          Services.prefs.getBoolPref(prefKey),
          prefValue,
          `${prefValue} is expected value`
        );
      }
      callbackCalled = true;
    }
  );

  ok(syncWindow, "Choose what to sync window opened");
  let syncChooseDialog = syncWindow.document.getElementById(
    "syncChooseOptions"
  );
  let syncCheckboxes = syncChooseDialog.querySelectorAll(
    "checkbox[preference]"
  );

  // This time we're adjusting to the cancel list
  [...syncCheckboxes].forEach(checkbox => {
    if (
      cancelSyncPrefs[checkbox.getAttribute("preference")] !== checkbox.checked
    ) {
      checkbox.click();
    }
  });

  syncChooseDialog.cancelDialog();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  Assert.ok(callbackCalled, "Cancel callback was called");
});
