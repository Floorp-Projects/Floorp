/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 *  Test removing bookmarks from the Bookmarks Toolbar and Library.
 */

const TEST_URL = "about:mozilla";

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();

  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  registerCleanupFunction(async () => {
    // Collapse the personal toolbar if needed.
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_remove_bookmark_from_toolbar() {
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Bookmark Title",
    url: TEST_URL,
  });

  let toolbarNode = getToolbarNodeForItemGuid(toolbarBookmark.guid);

  let contextMenu = document.getElementById("placesContext");
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  EventUtils.synthesizeMouseAtCenter(toolbarNode, {
    button: 2,
    type: "contextmenu",
  });
  await popupShownPromise;

  let contextMenuDeleteBookmark = document.getElementById(
    "placesContext_deleteBookmark"
  );

  let removePromise = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events => events.some(event => event.url == TEST_URL),
    "places"
  );

  contextMenu.activateItem(contextMenuDeleteBookmark, {});

  await removePromise;

  Assert.deepEqual(
    PlacesUtils.bookmarks.fetch({ url: TEST_URL }),
    {},
    "Should have removed the bookmark from the database"
  );
});

add_task(async function test_remove_bookmark_from_library() {
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

  // Insert bookmarks.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children,
  });

  // Open the Library and select the "UnfiledBookmarks".
  let library = await promiseLibrary("UnfiledBookmarks");

  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
  });

  let PO = library.PlacesOrganizer;

  Assert.equal(
    PlacesUtils.getConcreteItemGuid(PO._places.selectedNode),
    PlacesUtils.bookmarks.unfiledGuid,
    "Should have selected unfiled bookmarks."
  );

  let contextMenu = library.document.getElementById("placesContext");
  let contextMenuDeleteBookmark = library.document.getElementById(
    "placesContext_deleteBookmark"
  );

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  let firstColumn = library.ContentTree.view.columns[0];
  let firstBookmarkRect = library.ContentTree.view.getCoordsForCellItem(
    0,
    firstColumn,
    "bm0"
  );

  EventUtils.synthesizeMouse(
    library.ContentTree.view.body,
    firstBookmarkRect.x,
    firstBookmarkRect.y,
    { type: "contextmenu", button: 2 },
    library
  );

  await popupShownPromise;

  Assert.equal(
    library.ContentTree.view.result.root.childCount,
    3,
    "Number of bookmarks before removal is right"
  );

  let removePromise = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events => events.some(event => event.url == uris[0]),
    "places"
  );
  contextMenu.activateItem(contextMenuDeleteBookmark, {});

  await removePromise;

  Assert.equal(
    library.ContentTree.view.result.root.childCount,
    2,
    "Should have removed the bookmark from the display"
  );
});
