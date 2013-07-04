/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  waitForExplicitFinish();
  openLibrary(onLibraryReady);
}

function onLibraryReady(aLibrary) {
  let hierarchy = [ "AllBookmarks", "BookmarksMenu" ];

  let folder1 = PlacesUtils.bookmarks
                           .createFolder(PlacesUtils.bookmarksMenuFolderId,
                                         "Folder 1",
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
  hierarchy.push(folder1);
  let folder2 = PlacesUtils.bookmarks
                           .createFolder(folder1, "Folder 2",
                                         PlacesUtils.bookmarks.DEFAULT_INDEX);
  hierarchy.push(folder2);
  let bookmark = PlacesUtils.bookmarks
                            .insertBookmark(folder2, NetUtil.newURI("http://example.com/"),
                                            PlacesUtils.bookmarks.DEFAULT_INDEX,
                                            "Bookmark");

  registerCleanupFunction(function() {
    PlacesUtils.bookmarks.removeItem(folder1);
    aLibrary.close();
  });

  aLibrary.PlacesOrganizer.selectLeftPaneContainerByHierarchy(hierarchy);

  is(aLibrary.PlacesOrganizer._places.selectedNode.itemId, folder2,
     "Found the expected left pane selected node");

  is(aLibrary.ContentTree.view.view.nodeForTreeIndex(0).itemId, bookmark,
     "Found the expected right pane contents");

  finish();
}
