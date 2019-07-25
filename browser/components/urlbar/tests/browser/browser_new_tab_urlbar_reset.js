/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify that urlbar state is reset when opening a new tab, so searching for the
 * same text will reopen the results popup.
 */
add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    false
  );
  await promiseAutocompleteResultPopup("m");
  assertOpen();

  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    false
  );
  await promiseAutocompleteResultPopup("m");
  assertOpen();

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

function assertOpen() {
  Assert.equal(gURLBar.view.isOpen, true, "Should be showing the popup");
}
