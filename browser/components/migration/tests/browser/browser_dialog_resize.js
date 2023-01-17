/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the MigrationWizard resizes when opened inside of a
 * TabDialogBox, that it causes the containing TabDialogBox to resize
 * appropriately.
 */
add_task(async function test_migration_dialog_resize_tab_dialog_box() {
  let wizardReady = BrowserTestUtils.waitForEvent(
    window,
    "MigrationWizard:Ready"
  );
  let dialogPromise = TestUtils.topicObserved("subdialog-loaded");
  MigrationUtils.showMigrationWizard(window, {});
  let [dialogWin] = await dialogPromise;
  await wizardReady;

  let dialogBrowser = dialogWin.docShell.chromeEventHandler;
  let dialogBody = dialogWin.document.body;
  let wizard = dialogBody.querySelector("#wizard");
  let height = wizard.getBoundingClientRect().height;

  let browserResizePromise = new Promise(resolve => {
    let observer = new ResizeObserver(() => {
      observer.disconnect(dialogBrowser);
      Assert.ok(true, "TabDialogBox browser resized.");
      resolve();
    });
    observer.observe(dialogBrowser);
  });

  let newHeight = height + 100;
  wizard.style.height = newHeight + "px";
  await browserResizePromise;

  let dialogClosed = BrowserTestUtils.waitForEvent(window, "dialogclose");
  EventUtils.sendKey("ESCAPE");
  await dialogClosed;
});

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
