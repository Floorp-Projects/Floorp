/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* test that copying a non movable query or folder shortcut makes a new query with the same url, not a deep copy */

const QUERY_URL = "place:sort=8&maxResults=10";

add_task(async function copy_toolbar_shortcut() {
  await promisePlacesInitComplete();

  let library = await promiseLibrary();

  registerCleanupFunction(async () => {
    library.close();
    await PlacesUtils.bookmarks.eraseEverything();
  });

  library.PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");

  await promiseClipboard(function() {
    library.PlacesOrganizer._places.controller.copy();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  library.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  await library.ContentTree.view.controller.paste();

  let toolbarCopyNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  is(
    toolbarCopyNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
    "copy is still a folder shortcut"
  );

  await PlacesUtils.bookmarks.remove(toolbarCopyNode.bookmarkGuid);
  library.PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");
  is(
    library.PlacesOrganizer._places.selectedNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
    "original is still a folder shortcut"
  );
});

add_task(async function copy_mobile_shortcut() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.bookmarks.showMobileBookmarks", true]],
  });
  await promisePlacesInitComplete();

  let library = await promiseLibrary();

  registerCleanupFunction(async () => {
    library.close();
    await PlacesUtils.bookmarks.eraseEverything();
  });

  library.PlacesOrganizer.selectLeftPaneContainerByHierarchy([
    PlacesUtils.virtualAllBookmarksGuid,
    PlacesUtils.bookmarks.virtualMobileGuid,
  ]);

  await promiseClipboard(function() {
    library.PlacesOrganizer._places.controller.copy();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  library.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  await library.ContentTree.view.controller.paste();

  let mobileCopyNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  is(
    mobileCopyNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
    "copy is still a folder shortcut"
  );

  await PlacesUtils.bookmarks.remove(mobileCopyNode.bookmarkGuid);
  library.PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");
  is(
    library.PlacesOrganizer._places.selectedNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT,
    "original is still a folder shortcut"
  );
});

add_task(async function copy_history_query() {
  let library = await promiseLibrary();

  library.PlacesOrganizer.selectLeftPaneBuiltIn("History");

  await promiseClipboard(function() {
    library.PlacesOrganizer._places.controller.copy();
  }, PlacesUtils.TYPE_X_MOZ_PLACE);

  library.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");
  await library.ContentTree.view.controller.paste();

  let historyCopyNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  is(
    historyCopyNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
    "copy is still a query"
  );

  await PlacesUtils.bookmarks.remove(historyCopyNode.bookmarkGuid);
  library.PlacesOrganizer.selectLeftPaneBuiltIn("History");
  is(
    library.PlacesOrganizer._places.selectedNode.type,
    Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
    "original is still a query"
  );
});
