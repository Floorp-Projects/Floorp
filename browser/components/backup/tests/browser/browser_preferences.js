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
