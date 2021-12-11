/**
 *  Test searching for bookmarks (by title and by tag) from the Bookmarks sidebar.
 */
"use strict";

let sidebar = document.getElementById("sidebar");

const TEST_URI = "http://example.com/";
const BOOKMARKS_COUNT = 4;
const TEST_PARENT_FOLDER = "testParentFolder";
const TEST_SIF_URL = "http://testsif.example.com/";
const TEST_SIF_TITLE = "TestSIF";

function assertBookmarks(searchValue) {
  let found = 0;

  let searchBox = sidebar.contentDocument.getElementById("search-box");

  ok(searchBox, "search box is in context");

  searchBox.value = searchValue;
  searchBox.doCommand();

  let tree = sidebar.contentDocument.getElementById("bookmarks-view");

  for (let i = 0; i < tree.view.rowCount; i++) {
    let cellText = tree.view.getCellText(i, tree.columns.getColumnAt(0));

    if (cellText.includes("example page")) {
      found++;
    }
  }

  info("Reset the search");
  searchBox.value = "";
  searchBox.doCommand();

  is(found, BOOKMARKS_COUNT, "found expected site");
}

function showInFolder(aSearchStr, aParentFolderGuid) {
  let searchBox = sidebar.contentDocument.getElementById("search-box");

  searchBox.value = aSearchStr;
  searchBox.doCommand();

  let tree = sidebar.contentDocument.getElementById("bookmarks-view");
  let theNode = tree.view._getNodeForRow(0);
  let bookmarkGuid = theNode.bookmarkGuid;

  Assert.equal(theNode.uri, TEST_SIF_URL, "Found expected bookmark");

  info("Running Show in Folder command");
  tree.selectNode(theNode);
  tree.controller.doCommand("placesCmd_showInFolder");

  let treeNode = tree.selectedNode;
  Assert.equal(
    treeNode.parent.bookmarkGuid,
    aParentFolderGuid,
    "Containing folder node is correct"
  );
  Assert.equal(
    treeNode.bookmarkGuid,
    bookmarkGuid,
    "The searched bookmark guid matches selected node"
  );
  Assert.equal(
    treeNode.uri,
    TEST_SIF_URL,
    "The searched bookmark URL matches selected node"
  );
}

add_task(async function test() {
  // Add bookmarks and tags.
  for (let i = 0; i < BOOKMARKS_COUNT; i++) {
    let url = Services.io.newURI(TEST_URI + i);

    await PlacesUtils.bookmarks.insert({
      url,
      title: "example page " + i,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    });
    PlacesUtils.tagging.tagURI(url, ["test"]);
  }

  await withSidebarTree("bookmarks", function() {
    // Search a bookmark by its title.
    assertBookmarks("example.com");
    // Search a bookmark by its tag.
    assertBookmarks("test");
  });

  // Cleanup before testing Show in Folder.
  await PlacesUtils.bookmarks.eraseEverything();

  // Now test Show in Folder
  info("Test Show in Folder");
  let parentFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: TEST_PARENT_FOLDER,
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: parentFolder.guid,
    title: TEST_SIF_TITLE,
    url: TEST_SIF_URL,
  });

  await withSidebarTree("bookmarks", function() {
    showInFolder(TEST_SIF_TITLE, parentFolder.guid);
  });

  // Cleanup
  await PlacesUtils.bookmarks.eraseEverything();
});
