/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserTestUtils: "resource://testing-common/BrowserTestUtils.jsm",
});

// Check that the URL bar manages accessibility
// selection notifications appropriately on startup (new window).
async function runTests() {
  let focused = waitForEvent(
    EVENT_FOCUS,
    event => event.accessible.role == ROLE_ENTRY
  );
  info("Creating new window");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "addons",
    url: Services.io.newURI("http://www.addons.mozilla.org/"),
  });

  registerCleanupFunction(async function() {
    await BrowserTestUtils.closeWindow(newWin);
    await PlacesUtils.bookmarks.remove(bookmark);
  });
  info("Focusing window");
  newWin.focus();
  await focused;

  // Ensure the URL bar is ready for a new URL to be typed.
  // Sometimes, when this test runs, the existing text isn't selected when the
  // URL bar is focused. Pressing escape twice ensures that the popup is
  // closed and that the existing text is selected.
  EventUtils.synthesizeKey("KEY_Escape", {}, newWin);
  EventUtils.synthesizeKey("KEY_Escape", {}, newWin);
  let caretMoved = waitForEvent(
    EVENT_TEXT_CARET_MOVED,
    event => event.accessible.role == ROLE_ENTRY
  );

  info("Autofilling after typing `a` in new window URL bar.");
  EventUtils.synthesizeKey("a", {}, newWin);
  await UrlbarTestUtils.promiseSearchComplete(newWin);
  Assert.equal(
    newWin.gURLBar.inputField.value,
    "addons.mozilla.org/",
    "autofilled value as expected"
  );

  info("Ensuring caret moved on text selection");
  await caretMoved;
}

addAccessibleTask(``, runTests);
