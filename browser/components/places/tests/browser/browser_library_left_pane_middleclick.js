/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests middle-clicking items in the Library.
 */

const URIs = ["about:license", "about:mozilla"];

var gLibrary = null;

add_task(async function test_setup() {
  // Temporary disable history, so we won't record pages navigation.
  await SpecialPowers.pushPrefEnv({ set: [["places.history.enabled", false]] });

  // Open Library window.
  gLibrary = await promiseLibrary();

  registerCleanupFunction(async () => {
    // We must close "Other Bookmarks" ready for other tests.
    gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
    gLibrary.PlacesOrganizer._places.selectedNode.containerOpen = false;

    await PlacesUtils.bookmarks.eraseEverything();

    // Close Library window.
    await promiseLibraryClosed(gLibrary);
  });
});

add_task(async function test_open_folder_in_tabs() {
  let children = URIs.map(url => {
    return {
      title: "Title",
      url,
    };
  });

  // Create a new folder.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "Folder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children,
      },
    ],
  });

  // Select unsorted bookmarks root in the left pane.
  gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
  Assert.notEqual(
    gLibrary.PlacesOrganizer._places.selectedNode,
    null,
    "We correctly have selection in the Library left pane"
  );

  // Get our bookmark in the right pane.
  var folderNode = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
  Assert.equal(folderNode.title, "Folder", "Found folder in the right pane");

  gLibrary.PlacesOrganizer._places.selectedNode.containerOpen = true;

  // Now middle-click on the bookmark contained with it.
  let promiseLoaded = Promise.all(
    URIs.map(uri => BrowserTestUtils.waitForNewTab(gBrowser, uri, false, true))
  );

  let bookmarkedNode =
    gLibrary.PlacesOrganizer._places.selectedNode.getChild(0);
  mouseEventOnCell(
    gLibrary.PlacesOrganizer._places,
    gLibrary.PlacesOrganizer._places.view.treeIndexForNode(bookmarkedNode),
    0,
    { button: 1 }
  );

  let tabs = await promiseLoaded;

  Assert.ok(true, "Expected tabs were loaded");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

function mouseEventOnCell(aTree, aRowIndex, aColumnIndex, aEventDetails) {
  var selection = aTree.view.selection;
  selection.select(aRowIndex);
  aTree.ensureRowIsVisible(aRowIndex);
  var column = aTree.columns[aColumnIndex];

  // get cell coordinates
  var rect = aTree.getCoordsForCellItem(aRowIndex, column, "text");

  EventUtils.synthesizeMouse(
    aTree.body,
    rect.x,
    rect.y,
    aEventDetails,
    gLibrary
  );
}
