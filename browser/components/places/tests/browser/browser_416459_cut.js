/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "http://example.com/";

let gLibrary;
let gItemId;
let PlacesOrganizer;
let ContentTree;

function test() {
  waitForExplicitFinish();
  gLibrary = openLibrary(onLibraryReady);
}

function onLibraryReady() {
  PlacesOrganizer = gLibrary.PlacesOrganizer;
  ContentTree = gLibrary.ContentTree;

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in scope");
  ok(PlacesUIUtils, "PlacesUIUtils in scope");
  ok(PlacesOrganizer, "PlacesOrganizer in scope");
  ok(ContentTree, "ContentTree is in scope");

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
  ContentTree.view.selectItems([gItemId]);
  let bookmarkNode = ContentTree.view.selectedNode;
  is(bookmarkNode.uri, TEST_URL, "Found the expected bookmark");
}

function cutSelection() {
  info("Cutting selection");
  ContentTree.view.controller.cut();
}

function pasteClipboard(aLeftPaneQuery) {
  info("Selecting " + aLeftPaneQuery + " in the left pane");
  PlacesOrganizer.selectLeftPaneQuery(aLeftPaneQuery);
  info("Pasting clipboard");
  ContentTree.view.controller.paste();
}

function onClipboardReady() {
  pasteClipboard("UnfiledBookmarks");
  selectBookmarkIn("UnfiledBookmarks");

  // Cleanup.
  gLibrary.close();
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  finish();
}
