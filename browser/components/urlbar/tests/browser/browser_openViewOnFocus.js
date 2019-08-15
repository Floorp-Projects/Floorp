/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function checkOpensOnFocus(win = window) {
  Assert.ok(
    win.gURLBar.openViewOnFocusForCurrentTab,
    "openViewOnFocusForCurrentTab should be true"
  );
  // Even with openViewOnFocus = true, the view should not open when the input
  // is focused programmatically.
  win.gURLBar.blur();
  win.gURLBar.focus();
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  Assert.ok(
    BrowserTestUtils.is_hidden(UrlbarTestUtils.getDropMarker(win)),
    "The dropmarker should be hidden"
  );
  win.gURLBar.blur();
  Assert.ok(
    BrowserTestUtils.is_hidden(UrlbarTestUtils.getDropMarker(win)),
    "The dropmarker should be hidden"
  );
  // Check the keyboard shortcut.
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    win.document.getElementById("Browser:OpenLocation").doCommand();
  });
  win.gURLBar.blur();
  // Focus with the mouse.
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {});
  });
  win.gURLBar.blur();
}

function checkDoesNotOpenOnFocus(win = window) {
  Assert.ok(
    !win.gURLBar.openViewOnFocusForCurrentTab,
    "openViewOnFocusForCurrentTab should be false"
  );
  // The view should not open when the input is focused programmatically.
  win.gURLBar.blur();
  win.gURLBar.focus();
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  Assert.ok(
    BrowserTestUtils.is_hidden(UrlbarTestUtils.getDropMarker(win)),
    "The dropmarker should be hidden"
  );
  win.gURLBar.blur();
  Assert.ok(
    BrowserTestUtils.is_hidden(UrlbarTestUtils.getDropMarker(win)),
    "The dropmarker should be hidden"
  );
  // Check the keyboard shortcut.
  win.document.getElementById("Browser:OpenLocation").doCommand();
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  win.gURLBar.blur();
  // Focus with the mouse.
  EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {});
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
        // openViewOnFocus should be disabled for these pages even though the
        // pref is true.
        checkDoesNotOpenOnFocus();
        // Open a new tab where openViewOnFocus should be enabled.
        await BrowserTestUtils.withNewTab(
          { gBrowser, url: "http://example.com/" },
          async otherBrowser => {
            // openViewOnFocus should be enabled.
            await checkOpensOnFocus();
            // Switch back to about:newtab/home.  openViewOnFocus should be
            // disabled.
            await BrowserTestUtils.switchTab(
              gBrowser,
              gBrowser.getTabForBrowser(browser)
            );
            checkDoesNotOpenOnFocus();
            // Switch back to example.com.  openViewOnFocus should be enabled.
            await BrowserTestUtils.switchTab(
              gBrowser,
              gBrowser.getTabForBrowser(otherBrowser)
            );
            await checkOpensOnFocus();
          }
        );
        // After example.com closes, about:newtab/home should be selected again,
        // and openViewOnFocus should be disabled.
        checkDoesNotOpenOnFocus();
        // Load example.com in the same tab.  openViewOnFocus should become
        // enabled.
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
  checkDoesNotOpenOnFocus(privateWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
