"use strict";

/**
 * Verify that urlbar state is reset when openig a new tab, so searching for the
 * same text will reopen the results popup.
 */
add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  await promiseAutocompleteResultPopup("m");
  ok(gURLBar.popupOpen, "The popup is open");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  await promiseAutocompleteResultPopup("m");
  ok(gURLBar.popupOpen, "The popup is open");
  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});
