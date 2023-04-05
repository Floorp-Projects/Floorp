/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if browser.migrate.content-modal.about-welcome-behavior
 * is "autoclose", that closing the migration dialog when opened with the
 * NEWTAB entrypoint (which currently only occurs from about:welcome),
 * will result in the about:preferences tab closing too.
 */
add_task(async function test_autoclose_from_welcome() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.migrate.content-modal.about-welcome-behavior", "autoclose"],
    ],
  });

  let migrationDialogPromise = waitForMigrationWizardDialogTab();
  MigrationUtils.showMigrationWizard(window, {
    entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.NEWTAB,
  });

  let prefsBrowser = await migrationDialogPromise;
  let prefsTab = gBrowser.getTabForBrowser(prefsBrowser);

  let tabClosed = BrowserTestUtils.waitForTabClosing(prefsTab);

  let dialog = prefsBrowser.contentDocument.querySelector(
    "#migrationWizardDialog"
  );

  let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");
  await BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, prefsBrowser);
  await dialogClosed;
  await tabClosed;
  Assert.ok(true, "Preferences tab closed with autoclose behavior.");
});

/**
 * Tests that if browser.migrate.content-modal.about-welcome-behavior
 * is "default", that closing the migration dialog when opened with the
 * NEWTAB entrypoint (which currently only occurs from about:welcome),
 * will result in the about:preferences tab still staying open.
 */
add_task(async function test_no_autoclose_from_welcome() {
  // Create a new blank tab which about:preferences will open into.
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.migrate.content-modal.about-welcome-behavior", "default"]],
  });

  let migrationDialogPromise = waitForMigrationWizardDialogTab();
  MigrationUtils.showMigrationWizard(window, {
    entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.NEWTAB,
  });

  let prefsBrowser = await migrationDialogPromise;
  let prefsTab = gBrowser.getTabForBrowser(prefsBrowser);

  let dialog = prefsBrowser.contentDocument.querySelector(
    "#migrationWizardDialog"
  );

  let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");
  await BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, prefsBrowser);
  await dialogClosed;
  Assert.ok(!prefsTab.closing, "about:preferences tab is not closing.");

  BrowserTestUtils.removeTab(prefsTab);
});

/**
 * Tests that if browser.migrate.content-modal.about-welcome-behavior
 * is "standalone", that opening the migration wizard from the NEWTAB
 * entrypoint opens the migration wizard in a standalone top-level
 * window.
 */
add_task(async function test_no_autoclose_from_welcome() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.migrate.content-modal.about-welcome-behavior", "standalone"],
    ],
  });

  let windowOpened = BrowserTestUtils.domWindowOpened();
  MigrationUtils.showMigrationWizard(window, {
    entrypoint: MigrationUtils.MIGRATION_ENTRYPOINTS.NEWTAB,
  });
  let dialogWin = await windowOpened;
  Assert.ok(dialogWin, "Top-level dialog window opened for the migrator.");
  await BrowserTestUtils.waitForEvent(dialogWin, "MigrationWizard:Ready");

  let dialogClosed = BrowserTestUtils.domWindowClosed(dialogWin);
  dialogWin.close();
  await dialogClosed;
});
