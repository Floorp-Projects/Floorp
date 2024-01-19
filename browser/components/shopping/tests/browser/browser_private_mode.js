/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test verifies that the shopping sidebar is not initialized if the
// user visits a shopping product page while in private browsing mode.

add_task(async function test_private_window_disabled() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let browser = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(
    browser,
    "https://example.com/product/B09TJGHL5F"
  );
  await BrowserTestUtils.browserLoaded(browser);

  let shoppingButton = privateWindow.document.getElementById(
    "shopping-sidebar-button"
  );
  ok(
    BrowserTestUtils.isHidden(shoppingButton),
    "Shopping Button should not be visible on a product page"
  );

  ok(
    !privateWindow.document.querySelector("shopping-sidebar"),
    "Shopping sidebar does not exist"
  );

  await BrowserTestUtils.closeWindow(privateWindow);
});
