/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the cursor remains in the right place when a new window is opened.
 */

add_task(async function test_windowSwitch() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  await promiseAutocompleteResultPopup("www.mozilla.org");
  await waitForAutocompleteResultAt(0);

  gURLBar.focus();
  gURLBar.inputField.setSelectionRange(4, 4);

  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.closeWindow(newWindow);

  Assert.equal(
    document.activeElement,
    gURLBar.inputField,
    "URL Bar should be focused"
  );
  Assert.equal(gURLBar.selectionStart, 4, "Should not have moved the cursor");
  Assert.equal(gURLBar.selectionEnd, 4, "Should not have selected anything");

  BrowserTestUtils.removeTab(tab);
});
