/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test opening a flat container in the right pane even if its parent in the
 * left pane is closed.
 */

var library;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesUtils.history.clear();
    await promiseLibraryClosed(library);
  });
});

add_task(async function test_open_built_in_folder() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "testBM",
    url: "http://example.com/1",
  });

  library = await promiseLibrary("AllBookmarks");

  library.ContentTree.view.selectItems([PlacesUtils.bookmarks.menuGuid]);

  synthesizeClickOnSelectedTreeCell(library.ContentTree.view,
                                    { clickCount: 2 });

  Assert.equal(library.PlacesOrganizer._places.selectedNode.bookmarkGuid,
               PlacesUtils.bookmarks.virtualMenuGuid,
               "Should have the bookmarks menu selected in the left pane.");
  Assert.equal(library.ContentTree.view.view.nodeForTreeIndex(0).bookmarkGuid, bm.guid,
               "Should have the expected bookmark selected in the right pane");
});

add_task(async function test_open_new_folder_in_unfiled() {
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

  library.PlacesOrganizer.selectLeftPaneBuiltIn("UnfiledBookmarks");

  // Ensure the container is closed.
  library.PlacesOrganizer._places.selectedNode.containerOpen = false;

  let folderNode = library.ContentTree.view.view.nodeForTreeIndex(0);
  Assert.equal(folderNode.bookmarkGuid, bookmarks[0].guid,
               "Found the expected folder in the right pane");
  // Select the folder node in the right pane.
  library.ContentTree.view.selectNode(folderNode);

  synthesizeClickOnSelectedTreeCell(library.ContentTree.view,
                                    { clickCount: 2 });

  Assert.equal(library.ContentTree.view.view.nodeForTreeIndex(0).bookmarkGuid,
               bookmarks[1].guid,
               "Found the expected bookmark in the right pane");
});

add_task(async function test_open_history_query() {
  const todayTitle = PlacesUtils.getString("finduri-AgeInDays-is-0");
  await PlacesTestUtils.addVisits([{
    "uri": "http://example.com",
    "title": "Whittingtons"
  }]);

  library.PlacesOrganizer.selectLeftPaneBuiltIn("History");

  // Ensure the container is closed.
  library.PlacesOrganizer._places.selectedNode.containerOpen = false;

  let query = library.ContentTree.view.view.nodeForTreeIndex(0);
  Assert.equal(query.title, todayTitle, "Should have the today query");

  library.ContentTree.view.selectNode(query);

  synthesizeClickOnSelectedTreeCell(library.ContentTree.view,
                                    { clickCount: 2 });

  Assert.equal(library.PlacesOrganizer._places.selectedNode.title, todayTitle,
    "Should have selected the today query in the left-pane.");
  Assert.equal(library.ContentTree.view.view.nodeForTreeIndex(0).title,
               "Whittingtons",
               "Found the expected history item in the right pane");
});
