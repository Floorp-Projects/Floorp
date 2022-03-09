/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the a new bookmark is correctly selected after being created via
 * the bookmark dialog.
 */
"use strict";

let bookmarks = [
  {
    url: "https://example1.com",
    title: "bm1",
  },
  {
    url: "https://example2.com",
    title: "bm2",
  },
  {
    url: "https://example3.com",
    title: "bm3",
  },
];

add_task(async function test_open_bookmark_from_library_instantEdiBookmark() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.editDialog.delayedApply.enabled", false]],
  });

  let bm = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarks,
  });

  let library = await promiseLibrary("UnfiledBookmarks");

  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  let bmLibrary = library.ContentTree.view.view.nodeForTreeIndex(1);
  Assert.equal(
    bmLibrary.title,
    bm[1].title,
    "InstantEditBookmark: Found bookmark in the right pane"
  );

  library.ContentTree.view.selectNode(bmLibrary);

  await withBookmarksDialog(
    false,
    async () => {
      // Open the context menu.
      let placesContext = library.document.getElementById("placesContext");
      let promisePopup = BrowserTestUtils.waitForEvent(
        placesContext,
        "popupshown"
      );
      synthesizeClickOnSelectedTreeCell(library.ContentTree.view, {
        button: 2,
        type: "contextmenu",
      });

      await promisePopup;
      let properties = library.document.getElementById(
        "placesContext_new:bookmark"
      );
      placesContext.activateItem(properties, {});
    },
    async dialogWin => {
      let promiseLocationChange = PlacesTestUtils.waitForNotification(
        "bookmark-url-changed",
        events => events.some(e => e.url === "https://example4.com/"),
        "places"
      );

      fillBookmarkTextField(
        "editBMPanel_locationField",
        "https://example4.com/",
        dialogWin,
        false
      );

      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);

      await promiseLocationChange;
      let node = library.ContentTree.view.selectedNode;
      Assert.ok(node, "InstantEditBookmark: Should have a selectedNode");
      Assert.equal(
        node.uri,
        "https://example4.com/",
        "InstantEditBookmark: Should have selected the newly created bookmark"
      );
    }
  );
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_open_bookmark_from_library_editBookmark() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.editDialog.delayedApply.enabled", true]],
  });

  let bm = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarks,
  });

  let library = await promiseLibrary("UnfiledBookmarks");

  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  let bmLibrary = library.ContentTree.view.view.nodeForTreeIndex(1);
  Assert.equal(
    bmLibrary.title,
    bm[1].title,
    "EditBookmark: Found bookmark in the right pane"
  );

  library.ContentTree.view.selectNode(bmLibrary);

  await withBookmarksDialog(
    false,
    async () => {
      // Open the context menu.
      let placesContext = library.document.getElementById("placesContext");
      let promisePopup = BrowserTestUtils.waitForEvent(
        placesContext,
        "popupshown"
      );
      synthesizeClickOnSelectedTreeCell(library.ContentTree.view, {
        button: 2,
        type: "contextmenu",
      });

      await promisePopup;
      let properties = library.document.getElementById(
        "placesContext_new:bookmark"
      );
      placesContext.activateItem(properties, {});
    },
    async dialogWin => {
      fillBookmarkTextField(
        "editBMPanel_locationField",
        "https://example4.com/",
        dialogWin,
        false
      );

      EventUtils.synthesizeKey("VK_RETURN", {}, dialogWin);
    }
  );
  let node = library.ContentTree.view.selectedNode;
  Assert.ok(node, "EditBookmark: Should have a selectedNode");
  Assert.equal(
    node.uri,
    "https://example4.com/",
    "EditBookmark: Should have selected the newly created bookmark"
  );
  await PlacesUtils.bookmarks.eraseEverything();
});
