/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// These tests verify that the shopping sidebar is not initialized if the
// user visits a shopping product page while in private browsing mode.

const PRODUCT_PAGE = "https://example.com/product/Y4YM0Z1LL4";

let verifyButtonNotShown = win => {
  let shoppingButton = win.document.getElementById("shopping-sidebar-button");
  ok(
    BrowserTestUtils.isHidden(shoppingButton),
    "Shopping Button should not be visible on a product page"
  );
};

let verifySidebarNotShown = win => {
  ok(
    !win.document.querySelector("shopping-sidebar"),
    "Shopping sidebar does not exist"
  );
};

add_task(async function test_private_window_disabled() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let browser = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  verifyButtonNotShown(privateWindow);
  verifySidebarNotShown(privateWindow);

  await BrowserTestUtils.closeWindow(privateWindow);
});

// If a product page is open in a private window, and the feature is
// preffed on, the sidebar and button should not be shown in the private
// window (bug 1901979).
add_task(async function test_bug_1901979_pref_toggle_private_windows() {
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let browser = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, PRODUCT_PAGE);
  await BrowserTestUtils.browserLoaded(browser);

  verifyButtonNotShown(privateWindow);
  verifySidebarNotShown(privateWindow);

  // Flip the prefs to trigger the bug.
  Services.prefs.setBoolPref("browser.shopping.experience2023.enabled", false);
  Services.prefs.setBoolPref("browser.shopping.experience2023.enabled", true);

  // Verify we still haven't displayed the button or sidebar.
  verifyButtonNotShown(privateWindow);
  verifySidebarNotShown(privateWindow);

  await BrowserTestUtils.closeWindow(privateWindow);
});
