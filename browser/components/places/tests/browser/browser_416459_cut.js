/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com/";

add_task(function* () {
  yield PlacesUtils.bookmarks.eraseEverything();
  let organizer = yield promiseLibrary();

  registerCleanupFunction(function* () {
    yield promiseLibraryClosed(organizer);
    yield PlacesUtils.bookmarks.eraseEverything();
  });

  let PlacesOrganizer = organizer.PlacesOrganizer;
  let ContentTree = organizer.ContentTree;

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in scope");
  ok(PlacesUIUtils, "PlacesUIUtils in scope");
  ok(PlacesOrganizer, "PlacesOrganizer in scope");
  ok(ContentTree, "ContentTree is in scope");

  // Test with multiple entries to ensure they retain their order.
  let bookmarks = [];
  bookmarks.push(yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: TEST_URL,
    title: "0"
  }));
  bookmarks.push(yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: TEST_URL,
    title: "1"
  }));
  bookmarks.push(yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: TEST_URL,
    title: "2"
  }));

  yield selectBookmarksIn(organizer, bookmarks, "BookmarksToolbar");

  yield promiseClipboard(() => {
    info("Cutting selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
  info("Pasting clipboard");
  ContentTree.view.controller.paste();

  yield selectBookmarksIn(organizer, bookmarks, "UnfiledBookmarks");
});

var selectBookmarksIn = Task.async(function* (organizer, bookmarks, aLeftPaneQuery) {
  let PlacesOrganizer = organizer.PlacesOrganizer;
  let ContentTree = organizer.ContentTree;
  info("Selecting " + aLeftPaneQuery + " in the left pane");
  PlacesOrganizer.selectLeftPaneQuery(aLeftPaneQuery);

  let ids = [];
  for (let {guid} of bookmarks) {
    let bookmark = yield PlacesUtils.bookmarks.fetch(guid);
    is (bookmark.parentGuid, PlacesOrganizer._places.selectedNode.targetFolderGuid,
        "Bookmark has the right parent");
    ids.push(yield PlacesUtils.promiseItemId(bookmark.guid));
  }

  info("Selecting the bookmarks in the right pane");
  ContentTree.view.selectItems(ids);

  for (let node of ContentTree.view.selectedNodes) {
    is(node.bookmarkIndex, node.title,
       "Found the expected bookmark in the expected position");
  }
});
