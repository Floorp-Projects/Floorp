/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test opening a flat container in the right pane even if its parent in the
 * left pane is closed.
 */

add_task(async function() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "Folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "Bookmark",
        url: "http://example.com",
      }],
    }],
  });

  let library = await promiseLibrary("AllBookmarks");
  registerCleanupFunction(async function() {
    await promiseLibraryClosed(library);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  // Select unfiled later, to ensure it's closed.
  library.PlacesOrganizer.selectLeftPaneQuery("UnfiledBookmarks");
  ok(!library.PlacesOrganizer._places.selectedNode.containerOpen,
     "Unfiled container is closed");

  let folderNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  is(folderNode.bookmarkGuid, bookmarks[0].guid,
     "Found the expected folder in the right pane");
  // Select the folder node in the right pane.
  library.ContentTree.view.selectNode(folderNode);

  synthesizeClickOnSelectedTreeCell(library.ContentTree.view,
                                    { clickCount: 2 });

  is(library.ContentTree.view.view.nodeForTreeIndex(0).bookmarkGuid, bookmarks[1].guid,
     "Found the expected bookmark in the right pane");
});
