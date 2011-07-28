/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "http://example.com/";

let gLibrary;
let gItemId;
let PlacesOrganizer;

function test() {
  waitForExplicitFinish();
  gLibrary = openLibrary(onLibraryReady);
}

function onLibraryReady() {
  PlacesOrganizer = gLibrary.PlacesOrganizer;

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in scope");
  ok(PlacesUIUtils, "PlacesUIUtils in scope");
  ok(PlacesOrganizer, "PlacesOrganizer in scope");

  gItemId = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.toolbarFolderId, NetUtil.newURI(TEST_URL),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "test"
  );

  selectBookmarkIn("BookmarksToolbar");

  waitForClipboard(function(aData) !!aData,
                   cutSelection,
                   onClipboardReady,
                   PlacesUtils.TYPE_X_MOZ_PLACE);
}

function selectBookmarkIn(aLeftPaneQuery) {
  info("Selecting " + aLeftPaneQuery + " in the left pane");
  PlacesOrganizer.selectLeftPaneQuery(aLeftPaneQuery);
  let rootId = PlacesUtils.getConcreteItemId(PlacesOrganizer._places.selectedNode);
  is(PlacesUtils.bookmarks.getFolderIdForItem(gItemId), rootId,
     "Bookmark has the right parent");
  info("Selecting the bookmark in the right pane");
  PlacesOrganizer._content.selectItems([gItemId]);
  let bookmarkNode = PlacesOrganizer._content.selectedNode;
  is(bookmarkNode.uri, TEST_URL, "Found the expected bookmark");
}

function cutSelection() {
  info("Cutting selection");
  PlacesOrganizer._content.controller.cut();
}

function pasteClipboard(aLeftPaneQuery) {
  info("Selecting " + aLeftPaneQuery + " in the left pane");
  PlacesOrganizer.selectLeftPaneQuery(aLeftPaneQuery);
  info("Pasting clipboard");
  PlacesOrganizer._content.controller.paste();
}

function onClipboardReady() {
  pasteClipboard("UnfiledBookmarks");
  selectBookmarkIn("UnfiledBookmarks");

  // Cleanup.
  gLibrary.close();
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  finish();
}
