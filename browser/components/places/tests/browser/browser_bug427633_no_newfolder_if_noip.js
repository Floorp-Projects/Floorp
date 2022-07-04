"use strict";

/**
 * Bug 427633 - Disable creating a New Folder in the bookmarks dialogs if
 * insertionPoint is invalid.
 */

const TEST_URL = "about:buildconfig";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  registerCleanupFunction(async () => {
    bookmarkPanel.removeAttribute("animate");
    await BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  StarUI._createPanelIfNeeded();
  let bookmarkPanel = document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);

  let shownPromise = promisePopupShown(bookmarkPanel);

  let bookmarkStar = BookmarkingUI.star;
  bookmarkStar.click();

  await shownPromise;

  ok(gEditItemOverlay, "gEditItemOverlay is in context");
  ok(gEditItemOverlay.initialized, "gEditItemOverlay is initialized");

  window.gEditItemOverlay.toggleFolderTreeVisibility();

  let tree = gEditItemOverlay._element("folderTree");

  tree.view.selection.clearSelection();
  ok(
    document.getElementById("editBMPanel_newFolderButton").disabled,
    "New folder button is disabled if there's no selection"
  );

  let hiddenPromise = promisePopupHidden(bookmarkPanel);
  document.getElementById("editBookmarkPanelRemoveButton").click();
  await hiddenPromise;
});
