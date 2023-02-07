/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the MigrationWizard resizes when opened inside of a
 * XUL window, that it causes the containing XUL window to resize
 * appropriately.
 */
add_task(async function test_migration_dialog_resize_in_xul_window() {
  let windowOpened = BrowserTestUtils.domWindowOpened();
  MigrationUtils.showMigrationWizard(null, {});
  let dialogWin = await windowOpened;

  await BrowserTestUtils.waitForEvent(dialogWin, "MigrationWizard:Ready");

  let wizard = dialogWin.document.body.querySelector("#wizard");
  let height = wizard.getBoundingClientRect().height;

  let windowResizePromise = BrowserTestUtils.waitForEvent(dialogWin, "resize");
  wizard.style.height = height + 100 + "px";
  await windowResizePromise;
  Assert.ok(true, "Migration dialog window resized.");

  let dialogClosed = BrowserTestUtils.domWindowClosed(dialogWin);
  dialogWin.close();
  await dialogClosed;
});
