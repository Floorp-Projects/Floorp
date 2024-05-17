/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the section for controlling backup in about:preferences is
 * visible, but can also be hidden via a pref.
 */
add_task(async function test_preferences_visibility() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let backupSection =
      browser.contentDocument.querySelector("#dataBackupGroup");
    Assert.ok(backupSection, "Found backup preferences section");

    // Our mochitest-browser tests are configured to have the section visible
    // by default.
    Assert.ok(
      BrowserTestUtils.isVisible(backupSection),
      "Backup section is visible"
    );
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.backup.preferences.ui.enabled", false]],
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let backupSection =
      browser.contentDocument.querySelector("#dataBackupGroup");
    Assert.ok(backupSection, "Found backup preferences section");

    Assert.ok(
      BrowserTestUtils.isHidden(backupSection),
      "Backup section is now hidden"
    );
  });

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that the turn on scheduled backups dialog can set
 * browser.backup.scheduled.enabled to true from the settings page.
 */
add_task(async function test_turn_on_scheduled_backups_confirm() {
  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let settings = browser.contentDocument.querySelector("backup-settings");

    await settings.updateComplete;

    let turnOnButton = settings.scheduledBackupsButtonEl;

    Assert.ok(
      turnOnButton,
      "Button to turn on scheduled backups should be found"
    );

    turnOnButton.click();

    await settings.updateComplete;

    let turnOnScheduledBackups = settings.turnOnScheduledBackupsEl;

    Assert.ok(
      turnOnScheduledBackups,
      "turn-on-scheduled-backups should be found"
    );

    let confirmButton = turnOnScheduledBackups.confirmButtonEl;
    let promise = BrowserTestUtils.waitForEvent(
      window,
      "scheduledBackupsConfirm"
    );

    Assert.ok(confirmButton, "Confirm button should be found");

    confirmButton.click();

    await promise;
    await settings.updateComplete;

    let scheduledPrefVal = Services.prefs.getBoolPref(
      "browser.backup.scheduled.enabled"
    );
    Assert.ok(scheduledPrefVal, "Scheduled backups pref should be true");
  });
});
