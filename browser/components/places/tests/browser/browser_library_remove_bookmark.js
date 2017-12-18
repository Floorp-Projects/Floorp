/**
 * Test deleting bookmarks from library.
 */
"use strict";

add_task(async function test_remove_bookmark() {
  const uris = [
    "http://example.com/1",
    "http://example.com/2",
    "http://example.com/3",
  ];

  let children = uris.map((uri, index) => {
    return {
      title: `bm${index}`,
      url: uri,
    };
  });

  // Insert Bookmarks.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children
  });

  // Open the Library and select the "UnfiledBookmarks".
  let library = await promiseLibrary("UnfiledBookmarks");

  let PO = library.PlacesOrganizer;

  Assert.equal(PlacesUtils.getConcreteItemGuid(PO._places.selectedNode),
    PlacesUtils.bookmarks.unfiledGuid, "Should have selected unfiled bookmarks.");

  let contextMenu = library.document.getElementById("placesContext");
  let contextMenuDeleteItem = library.document.getElementById("placesContext_delete");

  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");

  let treeBoxObject = library.ContentTree.view.treeBoxObject;
  let firstColumn = library.ContentTree.view.columns[0];
  let firstBookmarkRect = treeBoxObject.getCoordsForCellItem(0, firstColumn, "bm0");

  EventUtils.synthesizeMouse(
    library.ContentTree.view.body,
    firstBookmarkRect.x,
    firstBookmarkRect.y,
    { type: "contextmenu", button: 2 },
    library
  );

  await popupShownPromise;

  Assert.equal(library.ContentTree.view.result.root.childCount, 3, "Number of bookmarks before removal is right");

  let removePromise = PlacesTestUtils.waitForNotification("onItemRemoved", (itemId, parentId, index, type, uri, guid) => uri.spec == uris[0]);
  EventUtils.synthesizeMouseAtCenter(contextMenuDeleteItem, {}, library);

  await removePromise;

  Assert.equal(library.ContentTree.view.result.root.childCount, 2, "Should have removed the bookmark from the display");

  // Cleanup
  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  });
});
