/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Bug 1435562 - Test that browser.tabs.warnOnOpen is respected when
 * opening multiple items from the Library. */

"use strict";

var gLibrary = null;

add_task(async function setup() {
  // Temporarily disable history, so we won't record pages navigation.
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

add_task(async function test_warnOnOpenFolder() {
  // Generate a list of links larger than browser.tabs.maxOpenBeforeWarn
  const MAX_LINKS = 16;
  let children = [];
  for (let i = 0; i < MAX_LINKS; i++) {
    children.push({
      title: `Folder Target ${i}`,
      url: `http://example${i}.com`,
    });
  }

  // Create a new folder containing our links.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "bigFolder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children,
      },
    ],
  });
  info("Pushed test folder into the bookmarks tree");

  // Select unsorted bookmarks root in the left pane.
  gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
  info("Got selection in the Library left pane");

  // Get our bookmark in the right pane.
  gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
  info("Got bigFolder in the right pane");

  gLibrary.PlacesOrganizer._places.selectedNode.containerOpen = true;

  // Middle-click on folder (opens all links in folder) and then cancel opening in the dialog
  let promiseLoaded = BrowserTestUtils.promiseAlertDialog("cancel");
  let bookmarkedNode = gLibrary.PlacesOrganizer._places.selectedNode.getChild(
    0
  );
  mouseEventOnCell(
    gLibrary.PlacesOrganizer._places,
    gLibrary.PlacesOrganizer._places.view.treeIndexForNode(bookmarkedNode),
    0,
    { button: 1 }
  );

  await promiseLoaded;

  Assert.ok(
    true,
    "Expected dialog was shown when attempting to open folder with lots of links"
  );

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_warnOnOpenLinks() {
  // Generate a list of links larger than browser.tabs.maxOpenBeforeWarn
  const MAX_LINKS = 16;
  let children = [];
  for (let i = 0; i < MAX_LINKS; i++) {
    children.push({
      title: `Highlighted Target ${i}`,
      url: `http://example${i}.com`,
    });
  }

  // Insert the links into the tree
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children,
  });
  info("Pushed test folder into the bookmarks tree");

  gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");
  info("Got selection in the Library left pane");

  // Select all the links
  gLibrary.ContentTree.view.selectAll();

  let placesContext = gLibrary.document.getElementById("placesContext");
  let promiseContextMenu = BrowserTestUtils.waitForEvent(
    placesContext,
    "popupshown"
  );

  // Open up the context menu and select "Open All In Tabs" (the first item in the list)
  synthesizeClickOnSelectedTreeCell(gLibrary.ContentTree.view, {
    button: 2,
    type: "contextmenu",
  });

  await promiseContextMenu;
  info("Context menu opened as expected");

  let openTabs = gLibrary.document.getElementById(
    "placesContext_openBookmarkLinks:tabs"
  );
  let promiseLoaded = BrowserTestUtils.promiseAlertDialog("cancel");

  placesContext.activateItem(openTabs, {});

  await promiseLoaded;

  Assert.ok(
    true,
    "Expected dialog was shown when attempting to open lots of selected links"
  );

  await PlacesUtils.bookmarks.eraseEverything();
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
