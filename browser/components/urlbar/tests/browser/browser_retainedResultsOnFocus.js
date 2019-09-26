/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests "megabar" redesign approach with retained results.
// When there is a pending search (user typed a search string and blurred
// without picking a result), on focus we should the search results again.

// Note: this is similar to openViewOnFocus, but has a different behavior:
// * Private browsing windows work the same as normal windows
// * Opens the panel only if there is a user typed search string

async function checkOpensOnFocus(win = window) {
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  win.gURLBar.blur();

  info("Check the keyboard shortcut.");
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    win.document.getElementById("Browser:OpenLocation").doCommand();
  });
  await UrlbarTestUtils.promisePopupClose(win, () => {
    win.gURLBar.blur();
  });
  info("Focus with the mouse.");
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {}, win);
  });
}

async function checkDoesNotOpenOnFocus(win = window) {
  Assert.ok(
    !win.gURLBar.openViewOnFocusForCurrentTab,
    "openViewOnFocusForCurrentTab should be false"
  );
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  win.gURLBar.blur();

  info("Check the keyboard shortcut.");
  win.document.getElementById("Browser:OpenLocation").doCommand();
  // Because the panel opening may not be immediate, we must wait a bit.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 500));
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
  win.gURLBar.blur();
  info("Focus with the mouse.");
  EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {}, win);
  // Because the panel opening may not be immediate, we must wait a bit.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 500));
  Assert.ok(!win.gURLBar.view.isOpen, "check urlbar panel is not open");
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.megabar", true]],
  });
  // Add some history for the empty panel.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://mochi.test:8888/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);
  registerCleanupFunction(PlacesUtils.history.clear);
});

async function test_window(win) {
  for (let url of ["about:newtab", "about:home", "http://example.com/"]) {
    // withNewTab may hang on preloaded pages, thus instead of waiting for load
    // we just wait for the expected currentURI value.
    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url, waitForLoad: false },
      async browser => {
        await TestUtils.waitForCondition(
          () => win.gBrowser.currentURI.spec == url,
          "Ensure we're on the expected page"
        );

        info("The panel should not open on the page by default");
        await checkDoesNotOpenOnFocus(win);

        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window: win,
          waitForFocus,
          value: "foo",
        });
        await UrlbarTestUtils.promisePopupClose(win, () => {
          win.gURLBar.blur();
        });

        info("The panel should open when there's a search string");
        await checkOpensOnFocus(win);

        await UrlbarTestUtils.promisePopupClose(win, () => {
          win.gURLBar.blur();
        });
      }
    );
  }
}

add_task(async function test_normalWindow() {
  // The megabar works properly in a new window.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await test_window(win);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function privateWindow() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await test_window(privateWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
