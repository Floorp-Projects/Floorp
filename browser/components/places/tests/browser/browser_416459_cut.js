/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com/";

add_task(function* () {
  let organizer = yield promiseLibrary();
  let PlacesOrganizer = organizer.PlacesOrganizer;
  let ContentTree = organizer.ContentTree;

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in scope");
  ok(PlacesUIUtils, "PlacesUIUtils in scope");
  ok(PlacesOrganizer, "PlacesOrganizer in scope");
  ok(ContentTree, "ContentTree is in scope");

  let bm = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: TEST_URL
  });

  yield selectBookmarkIn(organizer, bm, "BookmarksToolbar");

  yield promiseClipboard(() => {
    info("Cutting selection");
    ContentTree.view.controller.cut();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  info("Selecting UnfiledBookmarks in the left pane");
  PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
  info("Pasting clipboard");
  ContentTree.view.controller.paste();

  yield selectBookmarkIn(organizer, bm, "UnfiledBookmarks");

  yield promiseLibraryClosed(organizer);
  yield PlacesUtils.bookmarks.eraseEverything();
});

let selectBookmarkIn = Task.async(function* (organizer, bm, aLeftPaneQuery) {
  let PlacesOrganizer = organizer.PlacesOrganizer;
  let ContentTree = organizer.ContentTree;

  info("Selecting " + aLeftPaneQuery + " in the left pane");
  PlacesOrganizer.selectLeftPaneQuery(aLeftPaneQuery);
  let rootId = PlacesUtils.getConcreteItemId(PlacesOrganizer._places.selectedNode);

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  is((yield PlacesUtils.promiseItemId(bm.parentGuid)), rootId,
     "Bookmark has the right parent");

  info("Selecting the bookmark in the right pane");
  ContentTree.view.selectItems([yield PlacesUtils.promiseItemId(bm.guid)]);
  let bookmarkNode = ContentTree.view.selectedNode;
  is(bookmarkNode.uri, TEST_URL, "Found the expected bookmark");
});
