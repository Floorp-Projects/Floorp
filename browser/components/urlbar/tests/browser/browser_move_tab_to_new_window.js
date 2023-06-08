/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  These tests ensure that if the urlbar has a user typed value and the user
  moves the tab into a new window, the user typed value moves with it.
*/

add_setup(async function () {
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
  });
});

async function moveTabIntoNewWindowAndBack(url = "about:blank") {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  info("Replace urlbar value with a user typed value.");
  gURLBar.value = "hello world";
  UrlbarTestUtils.fireInputEvent(window);
  Assert.equal(
    gBrowser.userTypedValue,
    "hello world",
    "The user typed value should be replaced with hello world."
  );

  info("Move the tab into its own window.");
  let newWindow = gBrowser.replaceTabWithWindow(tab);
  let swapDocShellPromise = BrowserTestUtils.waitForEvent(
    tab.linkedBrowser,
    "SwapDocShells"
  );
  await swapDocShellPromise;
  Assert.equal(
    newWindow.gURLBar.value,
    "hello world",
    "The value of the urlbar should have been moved."
  );

  info("Return that tab back to its original window and select it.");
  tab = newWindow.gBrowser.selectedTab;
  swapDocShellPromise = BrowserTestUtils.waitForEvent(
    tab.linkedBrowser,
    "SwapDocShells"
  );
  gBrowser.adoptTab(newWindow.gBrowser.selectedTab, 1, true);
  await swapDocShellPromise;
  Assert.equal(
    gURLBar.value,
    "hello world",
    "The value of the urlbar should have been moved."
  );

  // Clean up.
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function move_newtab_with_value() {
  info("Open a new tab.");
  await moveTabIntoNewWindowAndBack();
});

add_task(async function move_loaded_page_with_value() {
  info("Open a new tab and load a URL.");
  await moveTabIntoNewWindowAndBack("https://www.example.com/");
});
