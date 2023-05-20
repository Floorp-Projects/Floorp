/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the a new bookmark is correctly selected after being created via
 * the bookmark dialog.
 */
"use strict";

const TEST_URIS = ["https://example1.com/", "https://example2.com/"];
let library;

add_setup(async function () {
  await PlacesTestUtils.addVisits(TEST_URIS);

  library = await promiseLibrary("History");

  registerCleanupFunction(async function () {
    await promiseLibraryClosed(library);
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_bookmark_page() {
  library.ContentTree.view.selectPlaceURI(TEST_URIS[0]);

  await withBookmarksDialog(
    true,
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
        "placesContext_createBookmark"
      );
      placesContext.activateItem(properties);
    },
    async dialogWin => {
      Assert.strictEqual(
        dialogWin.BookmarkPropertiesPanel._itemType,
        0,
        "Should have loaded a bookmark dialog"
      );
      Assert.equal(
        dialogWin.document.getElementById("editBMPanel_locationField").value,
        TEST_URIS[0],
        "Should have opened the dialog with the correct uri to be bookmarked"
      );
    }
  );
});

add_task(async function test_bookmark_pages() {
  library.ContentTree.view.selectAll();

  await withBookmarksDialog(
    true,
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
        "placesContext_createBookmark"
      );
      placesContext.activateItem(properties);
    },
    async dialogWin => {
      Assert.strictEqual(
        dialogWin.BookmarkPropertiesPanel._itemType,
        1,
        "Should have loaded a create bookmark folder dialog"
      );
      Assert.deepEqual(
        dialogWin.BookmarkPropertiesPanel._URIs.map(uri => uri.uri.spec),
        // The list here is reversed, because that's the order they're shown
        // in the view.
        [TEST_URIS[1], TEST_URIS[0]],
        "Should have got the correct URIs for adding to the folder"
      );
    }
  );
});
