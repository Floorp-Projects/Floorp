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
const TEST_NEW_TITLE = "NewTestSIF";

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

async function showInFolder(aSearchStr, aParentFolderGuid) {
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

  info("Check the title will be applied the item when changing it");
  await PlacesUtils.bookmarks.update({
    guid: theNode.bookmarkGuid,
    title: TEST_NEW_TITLE,
  });
  Assert.equal(
    treeNode.title,
    TEST_NEW_TITLE,
    "New title is applied to the node"
  );
}

add_task(async function testTree() {
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

  await withSidebarTree("bookmarks", function () {
    // Search a bookmark by its title.
    assertBookmarks("example.com");
    // Search a bookmark by its tag.
    assertBookmarks("test");
  });

  // Cleanup before testing Show in Folder.
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function testShowInFolder() {
  // Now test Show in Folder
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

  await withSidebarTree("bookmarks", async function () {
    await showInFolder(TEST_SIF_TITLE, parentFolder.guid);
  });

  // Cleanup
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function testRenameOnQueryResult() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: TEST_SIF_TITLE,
    url: TEST_SIF_URL,
  });

  await withSidebarTree("bookmarks", async function () {
    const searchBox = sidebar.contentDocument.getElementById("search-box");

    searchBox.value = TEST_SIF_TITLE;
    searchBox.doCommand();

    const tree = sidebar.contentDocument.getElementById("bookmarks-view");
    const theNode = tree.view._getNodeForRow(0);

    info("Check the found bookmark");
    Assert.equal(theNode.uri, TEST_SIF_URL, "URI of bookmark found is correct");
    Assert.equal(
      theNode.title,
      TEST_SIF_TITLE,
      "Title of bookmark found is correct"
    );

    info("Check the title will be applied the item when changing it");
    await PlacesUtils.bookmarks.update({
      guid: theNode.bookmarkGuid,
      title: TEST_NEW_TITLE,
    });

    // As the query result is refreshed once then the node also is regenerated,
    // need to get the result node from the tree again.
    Assert.equal(
      tree.view._getNodeForRow(0).bookmarkGuid,
      theNode.bookmarkGuid,
      "GUID of node regenerated is correct"
    );
    Assert.equal(
      tree.view._getNodeForRow(0).uri,
      theNode.uri,
      "URI of node regenerated is correct"
    );
    Assert.equal(
      tree.view._getNodeForRow(0).parentGuid,
      theNode.parentGuid,
      "parentGuid of node regenerated is correct"
    );
    Assert.equal(
      tree.view._getNodeForRow(0).title,
      TEST_NEW_TITLE,
      "New title is applied to the node"
    );

    info("Check the new date will be applied the item when changing it");
    const now = new Date();
    await PlacesUtils.bookmarks.update({
      guid: theNode.bookmarkGuid,
      dateAdded: now,
      lastModified: now,
    });

    Assert.equal(
      tree.view._getNodeForRow(0).uri,
      theNode.uri,
      "URI of node regenerated is correct"
    );
    Assert.equal(
      tree.view._getNodeForRow(0).dateAdded,
      now.getTime() * 1000,
      "New dateAdded is applied to the node"
    );
    Assert.equal(
      tree.view._getNodeForRow(0).lastModified,
      now.getTime() * 1000,
      "New lastModified is applied to the node"
    );
  });

  // Cleanup before testing Show in Folder.
  await PlacesUtils.bookmarks.eraseEverything();
});
