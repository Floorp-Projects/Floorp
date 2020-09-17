/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT_CHROME = getRootDirectory(gTestPath);
const TEST_DIALOG_PATH = TEST_ROOT_CHROME + "subdialog.xhtml";

/**
 * Tests that tab dialogs are focused when switching tabs.
 */
add_task(async function test_tabdialogbox_tab_switch_focus() {
  // Open 3 tabs
  let tabPromises = [];
  for (let i = 0; i < 3; i += 1) {
    tabPromises.push(
      BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        "http://example.com",
        true
      )
    );
  }

  // Wait for tabs to be ready
  let tabs = await Promise.all(tabPromises);

  // Open subdialog in first two tabs
  let dialogs = [];
  for (let i = 0; i < 2; i += 1) {
    let dialogBox = gBrowser.getTabDialogBox(tabs[i].linkedBrowser);
    dialogBox.open(TEST_DIALOG_PATH);
    dialogs.push(dialogBox._dialogManager._topDialog);
  }

  // Wait for dialogs to be ready
  await Promise.all([dialogs[0]._dialogReady, dialogs[1]._dialogReady]);

  // Switch to first tab which has dialog
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  // The textbox in the dialogs content window should be focused
  let dialogTextbox = dialogs[0]._frame.contentDocument.querySelector(
    "#textbox"
  );
  is(Services.focus.focusedElement, dialogTextbox, "Dialog textbox is focused");

  // Switch to second tab which has dialog
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);

  // The textbox in the dialogs content window should be focused
  let dialogTextbox2 = dialogs[1]._frame.contentDocument.querySelector(
    "#textbox"
  );
  is(
    Services.focus.focusedElement,
    dialogTextbox2,
    "Dialog2 textbox is focused"
  );

  // Switch to third tab which does not have a dialog
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);

  // Test that content is focused
  is(
    Services.focus.focusedElement,
    tabs[2].linkedBrowser,
    "Top level browser is focused"
  );

  // Cleanup
  tabs.forEach(tab => {
    BrowserTestUtils.removeTab(tab);
  });
});

/**
 * Tests that other dialogs are still visible if one dialog is hidden.
 */
add_task(async function test_tabdialogbox_tab_switch_hidden() {
  // Open 2 tabs
  let tabPromises = [];
  for (let i = 0; i < 2; i += 1) {
    tabPromises.push(
      BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        "http://example.com",
        true
      )
    );
  }

  // Wait for tabs to be ready
  let tabs = await Promise.all(tabPromises);

  // Open subdialog in tabs
  let dialogs = [];
  let dialogBox, dialogBoxManager, browser;
  for (let i = 0; i < 2; i += 1) {
    dialogBox = gBrowser.getTabDialogBox(tabs[i].linkedBrowser);
    browser = tabs[i].linkedBrowser;
    dialogBox.open(TEST_DIALOG_PATH);
    dialogBoxManager = dialogBox.getManager();
    dialogs.push(dialogBoxManager._topDialog);
  }

  // Wait for dialogs to be ready
  await Promise.all([dialogs[0]._dialogReady, dialogs[1]._dialogReady]);

  // Hide the top dialog
  dialogBoxManager.hideDialog(browser);

  is(dialogBoxManager._dialogStack.hidden, true, "Dialog stack is hidden");

  // Switch to first tab
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  // Check the dialog stack is showing in first tab
  dialogBoxManager = gBrowser
    .getTabDialogBox(tabs[0].linkedBrowser)
    .getManager();
  is(dialogBoxManager._dialogStack.hidden, false, "Dialog stack is showing");

  // Cleanup
  tabs.forEach(tab => {
    BrowserTestUtils.removeTab(tab);
  });
});
