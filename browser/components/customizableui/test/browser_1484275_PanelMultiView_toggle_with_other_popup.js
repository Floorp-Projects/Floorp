/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html,<html><body></body></html>";

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
    async function (browser) {
      // 1. Open the main menu.
      await gCUITestUtils.openMainMenu();

      // 2. Open another popup not managed by PanelMultiView.
      StarUI._createPanelIfNeeded();
      let bookmarkPanel = document.getElementById("editBookmarkPanel");
      let shown = BrowserTestUtils.waitForEvent(bookmarkPanel, "popupshown");
      let hidden = BrowserTestUtils.waitForEvent(bookmarkPanel, "popuphidden");
      EventUtils.synthesizeKey("D", { accelKey: true });
      await shown;

      // 3. Click the button to which the main menu is anchored. We need a native
      // mouse event to simulate the exact platform behavior with popups.
      let clickFn = () =>
        EventUtils.promiseNativeMouseEventAndWaitForEvent({
          type: "click",
          target: document.getElementById("PanelUI-button"),
          atCenter: true,
          eventTypeToWait: "mouseup",
        });

      // On Windows and macOS, the operation will close both popups.
      if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
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
