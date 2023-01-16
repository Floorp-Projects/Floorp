/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DIALOG_URL = "chrome://browser/content/migration/migration-dialog.html";

/**
 * Tests that pressing "Cancel" from the selection page of the migration
 * dialog closes the dialog when opened in a tab dialog.
 */
add_task(async function test_cancel_close() {
  let dialogBox = gBrowser.getTabDialogBox(gBrowser.selectedBrowser);
  let wizardReady = BrowserTestUtils.waitForEvent(
    window,
    "MigrationWizard:Ready"
  );
  let dialogPromise = dialogBox.open(DIALOG_URL);
  let dialogs = dialogBox.getTabDialogManager()._dialogs;

  Assert.equal(dialogs.length, 1, "Dialog manager has a dialog.");
  let dialog = dialogs[0];

  info("Waiting for dialogs to open.");
  await dialog._dialogReady;
  await wizardReady;

  let dialogBody = dialog._frame.contentDocument.body;

  let wizard = dialogBody.querySelector("#wizard");
  let shadow = wizard.openOrClosedShadowRoot;
  let cancelButton = shadow.querySelector(
    'div[name="page-selection"] .cancel-close'
  );

  cancelButton.click();
  await dialogPromise.closedPromise;

  Assert.equal(dialogs.length, 0, "No dialogs remain open.");
});

/**
 * Tests that pressing "Cancel" from the selection page of the migration
 * dialog closes the dialog when opened in stand-alone window.
 */
add_task(async function test_cancel_close() {
  let promiseWinLoaded = BrowserTestUtils.domWindowOpened().then(win => {
    return BrowserTestUtils.waitForEvent(win, "MigrationWizard:Ready");
  });

  let win = Services.ww.openWindow(
    window,
    DIALOG_URL,
    "_blank",
    "dialog,centerscreen",
    {}
  );
  await promiseWinLoaded;

  win.sizeToContent();
  let wizard = win.document.querySelector("#wizard");
  let shadow = wizard.openOrClosedShadowRoot;
  let cancelButton = shadow.querySelector(
    'div[name="page-selection"] .cancel-close'
  );

  let windowClosed = BrowserTestUtils.windowClosed(win);
  cancelButton.click();
  await windowClosed;
  Assert.ok(true, "Window was closed.");
});
