/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function checkOpensOnFocus(win = window) {
  // Even with openViewOnFocus = true, the view should not open when the input
  // is focused programmatically.
  win.gURLBar.blur();
  win.gURLBar.focus();
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  Assert.ok(win.gURLBar.dropmarker.hidden, "The dropmarker should be hidden");
  win.gURLBar.blur();
  Assert.ok(win.gURLBar.dropmarker.hidden, "The dropmarker should be hidden");

  // Check the keyboard shortcut.
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    win.document.getElementById("Browser:OpenLocation").doCommand();
  });
  await UrlbarTestUtils.promisePopupClose(win, () => {
    win.gURLBar.blur();
  });

  // Focus with the mouse.
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {});
  });
  await UrlbarTestUtils.promisePopupClose(win, () => {
    win.gURLBar.blur();
  });
}

async function checkDoesNotOpenOnFocus(win = window) {
  // The view should not open when the input is focused programmatically.
  win.gURLBar.blur();
  win.gURLBar.focus();
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  Assert.ok(win.gURLBar.dropmarker.hidden, "The dropmarker should be hidden");
  win.gURLBar.blur();
  Assert.ok(win.gURLBar.dropmarker.hidden, "The dropmarker should be hidden");

  // Check the keyboard shortcut.
  win.document.getElementById("Browser:OpenLocation").doCommand();
  // Because the panel opening may not be immediate, we must wait a bit.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 500));
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  win.gURLBar.blur();

  // Focus with the mouse.
  EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {});
  // Because the panel opening may not be immediate, we must wait a bit.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 500));
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  win.gURLBar.blur();
}

add_task(async function setUp() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.openViewOnFocus", true]],
  });
  // Add some history for the empty panel.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://mochi.test:8888/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);
  registerCleanupFunction(() => PlacesUtils.history.clear());
});

add_task(async function test() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      await checkOpensOnFocus();
    }
  );
});

add_task(async function newtabAndHome() {
  for (let url of ["about:newtab", "about:home"]) {
    // withNewTab randomly hangs on these pages when waitForLoad = true (the
    // default), so pass false.
    await BrowserTestUtils.withNewTab(
      { gBrowser, url, waitForLoad: false },
      async browser => {
        // We don't wait for load, but we must ensure to be on the expected url.
        await TestUtils.waitForCondition(
          () => window.gBrowser.currentURI.spec == url,
          "Ensure we're on the expected page"
        );
        await checkOpensOnFocus();
        await BrowserTestUtils.withNewTab(
          { gBrowser, url: "http://example.com/" },
          async otherBrowser => {
            await checkOpensOnFocus();
            // Switch back to about:newtab/home.
            await BrowserTestUtils.switchTab(
              gBrowser,
              gBrowser.getTabForBrowser(browser)
            );
            await checkOpensOnFocus();
            // Switch back to example.com.
            await BrowserTestUtils.switchTab(
              gBrowser,
              gBrowser.getTabForBrowser(otherBrowser)
            );
            await checkOpensOnFocus();
          }
        );
        // After example.com closes, about:newtab/home is selected again.
        await checkOpensOnFocus();
        // Load example.com in the same tab.
        await BrowserTestUtils.loadURI(
          gBrowser.selectedBrowser,
          "http://example.com/"
        );
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        await checkOpensOnFocus();
      }
    );
  }
});

add_task(async function privateWindow() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await checkDoesNotOpenOnFocus(privateWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
