/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Checks whether or not a row is visible when a user clicks on
 * row that's close to the bottom of the Library window, as the
 * DetailsPane can grow after the user has clicked on a bookmark.
 *
 */
"use strict";

let library;
let PlacesOrganizer;

function assertSelectedRowIsVisible(selectedRow, msg) {
  let firstRow = library.ContentTree.view.getFirstVisibleRow();
  let lastRow = library.ContentTree.view.getLastVisibleRow();
  Assert.ok(firstRow <= selectedRow && lastRow >= selectedRow, msg);
}

/**
 * Add a bunch of bookmarks so that the Library view needs to be
 * scrolled in order to see all the bookmarks.
 */
add_setup(async function() {
  await PlacesUtils.bookmarks.eraseEverything();

  library = await promiseLibrary("UnfiledBookmarks");

  let baseUrl = "https://www.example.com/";

  // Enough bookmarks that should go beyond the initial screen
  let nBookmarks = library.ContentTree.view.getLastVisibleRow() + 5;

  let bookmarks = new Array(nBookmarks);

  // Hack to make it so that when the list of bookmarks is
  // first loaded, a bookmark folder is first selected so that
  // the details pane is small
  bookmarks[0] = {
    title: "Test Folder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  };

  for (let index = 1; index < nBookmarks; ++index) {
    bookmarks[index] = {
      title: `Example Bookmark ${index + 10}`,
      url: baseUrl + index + 10,
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    };
  }

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarks,
  });

  await promiseLibraryClosed(library);

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    await promiseLibraryClosed(library);
  });
});

/**
 * Click on a bookmark that should be hidden when the details
 * panel is expanded, and so the library panel should scroll.
 */
add_task(async function test_click_bookmark() {
  library = await promiseLibrary("UnfiledBookmarks");

  let selectedRow = library.ContentTree.view.getLastVisibleRow();

  let node = library.ContentTree.view.view.nodeForTreeIndex(selectedRow);
  library.ContentTree.view.selectNode(node);
  synthesizeClickOnSelectedTreeCell(library.ContentTree.view);

  // TODO Bug 1769312: Have to wait till the row is scrolled to
  // which unfortunately is not easy to know at the current moment.
  // Should be replaced with an event.
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => {
    setTimeout(resolve, 500);
  });

  assertSelectedRowIsVisible(selectedRow, "Selected row is visible");

  await promiseLibraryClosed(library);
});

/**
 * Sort a bookmarks list by one of the columns and check if
 * clicking on a bookmark will show the bookmark.
 */
add_task(async function test_click_bookmark_on_sort() {
  library = await promiseLibrary("UnfiledBookmarks");

  let selectedRow = library.ContentTree.view.getLastVisibleRow();

  // Re-sort by name
  library.ViewMenu.setSortColumn(0, "descending");

  let node = library.ContentTree.view.view.nodeForTreeIndex(selectedRow);
  library.ContentTree.view.selectNode(node);
  synthesizeClickOnSelectedTreeCell(library.ContentTree.view);

  // TODO Bug 1769312: Have to wait till the row is scrolled to
  // which unfortunately is not easy to know at the current moment.
  // Should be replaced with an event.
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => {
    setTimeout(resolve, 500);
  });

  assertSelectedRowIsVisible(selectedRow, "Selected row is visible after sort");
});
