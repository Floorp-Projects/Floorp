/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 *  Tests that Library handles correctly batch deletes.
 */

const TEST_URL = "http://www.batch.delete.me/";

var gLibrary;

add_task(async function test_setup() {
  gLibrary = await promiseLibrary();

  registerCleanupFunction(function() {
    PlacesUtils.bookmarks
               .removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    // Close Library window.
    gLibrary.close();
  });
});

add_task(async function test_create_and_batch_remove_bookmarks() {
  let testURI = makeURI(TEST_URL);
  PlacesUtils.history.runInBatchMode({
    runBatched(aUserData) {
      // Create a folder in unserted and populate it with bookmarks.
      let folder = PlacesUtils.bookmarks.createFolder(
        PlacesUtils.unfiledBookmarksFolderId, "deleteme",
        PlacesUtils.bookmarks.DEFAULT_INDEX
      );
      PlacesUtils.bookmarks.createFolder(
        PlacesUtils.unfiledBookmarksFolderId, "keepme",
        PlacesUtils.bookmarks.DEFAULT_INDEX
      );
      for (let i = 0; i < 10; i++) {
        PlacesUtils.bookmarks.insertBookmark(folder,
                                             testURI,
                                             PlacesUtils.bookmarks.DEFAULT_INDEX,
                                             "bm" + i);
      }
    }
  }, null);

  // Select and open the left pane "History" query.
  let PO = gLibrary.PlacesOrganizer;
  PO.selectLeftPaneQuery("UnfiledBookmarks");
  Assert.notEqual(PO._places.selectedNode, null, "Selected unsorted bookmarks");

  let unsortedNode = PlacesUtils.asContainer(PO._places.selectedNode);
  unsortedNode.containerOpen = true;
  Assert.equal(unsortedNode.childCount, 2, "Unsorted node has 2 children");
  let folderNode = unsortedNode.getChild(0);
  Assert.equal(folderNode.title, "deleteme", "Folder found in unsorted bookmarks");
  // Check delete command is available.
  PO._places.selectNode(folderNode);
  Assert.equal(PO._places.selectedNode.title, "deleteme", "Folder node selected");
  Assert.ok(PO._places.controller.isCommandEnabled("cmd_delete"),
     "Delete command is enabled");
  // Execute the delete command and check bookmark has been removed.
  PO._places.controller.doCommand("cmd_delete");
  Assert.ok(!PlacesUtils.bookmarks.isBookmarked(testURI),
    "Bookmark has been correctly removed");
  // Test live update.
  Assert.equal(unsortedNode.childCount, 1, "Unsorted node has 1 child");
  Assert.equal(PO._places.selectedNode.title, "keepme", "Folder node selected");
  unsortedNode.containerOpen = false;
});

add_task(async function test_ensure_correct_selection_and_functionality() {
  let PO = gLibrary.PlacesOrganizer;
  let ContentTree = gLibrary.ContentTree;
  // Move selection forth and back.
  PO.selectLeftPaneQuery("History");
  PO.selectLeftPaneQuery("UnfiledBookmarks");
  // Now select the "keepme" folder in the right pane and delete it.
  ContentTree.view.selectNode(ContentTree.view.result.root.getChild(0));
  Assert.equal(ContentTree.view.selectedNode.title, "keepme",
    "Found folder in content pane");
  // Test live update.
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       makeURI(TEST_URL),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bm");
  Assert.equal(ContentTree.view.result.root.childCount, 2,
    "Right pane was correctly updated");
});
