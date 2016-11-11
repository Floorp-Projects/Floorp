/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* test that copying a non movable query or folder shortcut makes a new query with the same url, not a deep copy */

const SHORTCUT_URL = "place:folder=2";
const QUERY_URL = "place:sort=8&maxResults=10";

add_task(function* copy_toolbar_shortcut() {
  let library = yield promiseLibrary();

  registerCleanupFunction(function() {
    library.close();
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  });

  library.PlacesOrganizer.selectLeftPaneQuery("BookmarksToolbar");

  yield promiseClipboard(function() { library.PlacesOrganizer._places.controller.copy(); },
                         PlacesUtils.TYPE_X_MOZ_PLACE);

  library.PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
  library.ContentTree.view.controller.paste();

  let toolbarCopyNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  is(toolbarCopyNode.type,
     Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
     "copy is still a folder shortcut");

  PlacesUtils.bookmarks.removeItem(toolbarCopyNode.itemId);
  library.PlacesOrganizer.selectLeftPaneQuery("BookmarksToolbar");
  is(library.PlacesOrganizer._places.selectedNode.type,
     Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
     "original is still a folder shortcut");
});

add_task(function* copy_history_query() {
  let library = yield promiseLibrary();

  library.PlacesOrganizer.selectLeftPaneQuery("History");

  yield promiseClipboard(function() { library.PlacesOrganizer._places.controller.copy(); },
                         PlacesUtils.TYPE_X_MOZ_PLACE);

  library.PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
  library.ContentTree.view.controller.paste();

  let historyCopyNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  is(historyCopyNode.type,
     Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
     "copy is still a query");

  PlacesUtils.bookmarks.removeItem(historyCopyNode.itemId);
  library.PlacesOrganizer.selectLeftPaneQuery("History");
  is(library.PlacesOrganizer._places.selectedNode.type,
     Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
     "original is still a query");
});
