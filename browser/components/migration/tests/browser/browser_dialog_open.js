/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that we can open the migration dialog in an about:preferences
 * HTML5 dialog when calling MigrationUtils.showMigrationWizard within a
 * tabbrowser window execution context.
 */
add_task(async function test_migration_dialog_open_in_tab_dialog_box() {
  let migrationDialogPromise = waitForMigrationWizardDialogTab();
  MigrationUtils.showMigrationWizard(window, {});
  let prefsBrowser = await migrationDialogPromise;
  Assert.ok(true, "Migration dialog was opened");
  let dialog = prefsBrowser.contentDocument.querySelector(
    "#migrationWizardDialog"
  );

  let dialogClosed = BrowserTestUtils.waitForEvent(dialog, "close");
  await BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, prefsBrowser);
  await dialogClosed;
  BrowserTestUtils.startLoadingURIString(prefsBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(prefsBrowser);
});

/**
 * Tests that we can open the migration dialog in a stand-alone window
 * when calling MigrationUtils.showMigrationWizard with a null opener
 * argument, or a non-tabbrowser window context.
 */
add_task(async function test_migration_dialog_open_in_xul_window() {
  let firstWindowOpened = BrowserTestUtils.domWindowOpened();
  MigrationUtils.showMigrationWizard(null, {});
  let firstDialogWin = await firstWindowOpened;

  await BrowserTestUtils.waitForEvent(firstDialogWin, "MigrationWizard:Ready");

  Assert.ok(true, "Migration dialog was opened");

  // Now open a second migration dialog, using the first as the window
  // argument.

  let secondWindowOpened = BrowserTestUtils.domWindowOpened();
  MigrationUtils.showMigrationWizard(firstDialogWin, {});
  let secondDialogWin = await secondWindowOpened;

  await BrowserTestUtils.waitForEvent(secondDialogWin, "MigrationWizard:Ready");

  for (let dialogWin of [firstDialogWin, secondDialogWin]) {
    let dialogClosed = BrowserTestUtils.domWindowClosed(dialogWin);
    dialogWin.close();
    await dialogClosed;
  }
});
