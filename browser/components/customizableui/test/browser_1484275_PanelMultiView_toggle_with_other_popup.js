/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html,<html><body></body></html>";

// This code can be consolidated in the EventUtils module (bug 1126772).
const isWindows = AppConstants.platform == "win";
const isMac = AppConstants.platform == "macosx";
const mouseDown = isWindows ? 2 : isMac ? 1 : 4; // eslint-disable-line no-nested-ternary
const mouseUp = isWindows ? 4 : isMac ? 2 : 7; // eslint-disable-line no-nested-ternary
const utils = window.windowUtils;
const scale = utils.screenPixelsPerCSSPixel;
function synthesizeNativeMouseClick(aElement) {
  let rect = aElement.getBoundingClientRect();
  let win = aElement.ownerGlobal;
  let x = win.mozInnerScreenX + (rect.left + rect.right) / 2;
  let y = win.mozInnerScreenY + (rect.top + rect.bottom) / 2;

  // Wait for the mouseup event to occur before continuing.
  return new Promise((resolve, reject) => {
    function eventOccurred(e) {
      aElement.removeEventListener("mouseup", eventOccurred, true);
      resolve();
    }

    aElement.addEventListener("mouseup", eventOccurred, true);

    utils.sendNativeMouseEvent(x * scale, y * scale, mouseDown, 0, null);
    utils.sendNativeMouseEvent(x * scale, y * scale, mouseUp, 0, null);
  });
}

/**
 * Test steps that may lead to the panel being stuck on Windows (bug 1484275).
 */
add_task(async function test_PanelMultiView_toggle_with_other_popup() {
  // For proper cleanup, create a bookmark that we will remove later.
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
  });
  registerCleanupFunction(() => PlacesUtils.bookmarks.remove(bookmark));

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL,
    },
    async function(browser) {
      // 1. Open the main menu.
      await gCUITestUtils.openMainMenu();

      // 2. Open another popup not managed by PanelMultiView.
      let bookmarkPanel = document.getElementById("editBookmarkPanel");
      let shown = BrowserTestUtils.waitForEvent(bookmarkPanel, "popupshown");
      let hidden = BrowserTestUtils.waitForEvent(bookmarkPanel, "popuphidden");
      EventUtils.synthesizeKey("D", { accelKey: true });
      await shown;

      // 3. Click the button to which the main menu is anchored. We need a native
      // mouse event to simulate the exact platform behavior with popups.
      let clickFn = () =>
        synthesizeNativeMouseClick(document.getElementById("PanelUI-button"));

      if (AppConstants.platform == "win") {
        // On Windows, the operation will close both popups.
        await gCUITestUtils.hidePanelMultiView(PanelUI.panel, clickFn);
        await new Promise(resolve => executeSoon(resolve));

        // 4. Test that the popup can be opened again after it's been closed.
        await gCUITestUtils.openMainMenu();
        Assert.equal(PanelUI.panel.state, "open");
      } else {
        // On other platforms, the operation will close both popups and reopen the
        // main menu immediately, so we wait for the reopen to occur.
        shown = BrowserTestUtils.waitForEvent(PanelUI.mainView, "ViewShown");
        clickFn();
        await shown;
      }

      await gCUITestUtils.hideMainMenu();

      // Make sure the events for the bookmarks panel have also been processed
      // before closing the tab and removing the bookmark.
      await hidden;
    }
  );
});
