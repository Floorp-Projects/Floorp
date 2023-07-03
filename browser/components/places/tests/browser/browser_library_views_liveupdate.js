/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests Library Left pane view for liveupdate.
 */

let gLibrary = null;

add_setup(async function () {
  gLibrary = await promiseLibrary();
  await PlacesUtils.bookmarks.eraseEverything();

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    await promiseLibraryClosed(gLibrary);
  });
});

async function testInFolder(folderGuid, prefix) {
  let addedBookmarks = [];

  let item = await insertAndCheckItem(
    {
      parentGuid: folderGuid,
      title: `${prefix}1`,
      url: `http://${prefix}1.mozilla.org/`,
    },
    0
  );
  item.title = `${prefix}1_edited`;
  await updateAndCheckItem(item, 0);
  addedBookmarks.push(item);

  item = await insertAndCheckItem(
    {
      parentGuid: folderGuid,
      title: `${prefix}2`,
      url: "place:",
    },
    0
  );

  item.title = `${prefix}2_edited`;
  await updateAndCheckItem(item, 0);
  addedBookmarks.push(item);

  item = await insertAndCheckItem(
    {
      parentGuid: folderGuid,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    },
    2
  );
  addedBookmarks.push(item);

  item = await insertAndCheckItem(
    {
      parentGuid: folderGuid,
      title: `${prefix}f`,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    },
    1
  );

  item.title = `${prefix}f_edited`;
  await updateAndCheckItem(item, 1);

  item.index = 0;
  await updateAndCheckItem(item, 0);
  addedBookmarks.push(item);

  let folderGuid1 = item.guid;

  item = await insertAndCheckItem(
    {
      parentGuid: folderGuid1,
      title: `${prefix}f1`,
      url: `http://${prefix}f1.mozilla.org/`,
    },
    0
  );
  addedBookmarks.push(item);

  item = await insertAndCheckItem(
    {
      parentGuid: folderGuid,
      title: `${prefix}f12`,
      url: `http://${prefix}f12.mozilla.org/`,
    },
    4
  );
  addedBookmarks.push(item);

  // Move to a different folder and index.
  item.parentGuid = folderGuid1;
  item.index = 0;
  await updateAndCheckItem(item, 0);

  return addedBookmarks;
}

add_task(async function test() {
  let addedBookmarks = [];

  info("*** Acting on menu bookmarks");
  addedBookmarks = addedBookmarks.concat(
    await testInFolder(PlacesUtils.bookmarks.menuGuid, "bm")
  );

  info("*** Acting on toolbar bookmarks");
  addedBookmarks = addedBookmarks.concat(
    await testInFolder(PlacesUtils.bookmarks.toolbarGuid, "tb")
  );

  info("*** Acting on unsorted bookmarks");
  addedBookmarks = addedBookmarks.concat(
    await testInFolder(PlacesUtils.bookmarks.unfiledGuid, "ub")
  );

  // Remove bookmarks in reverse order, so that the effects are correct.
  for (let i = addedBookmarks.length - 1; i >= 0; i--) {
    await removeAndCheckItem(addedBookmarks[i]);
  }
});

function selectItem(item, parentTreeIndex) {
  let useLeftPane =
    item.type == PlacesUtils.bookmarks.TYPE_FOLDER || // is Folder
    (item.type == PlacesUtils.bookmarks.TYPE_BOOKMARK &&
      item.url.protocol == "place:"); // is Query
  let tree = useLeftPane
    ? gLibrary.PlacesOrganizer._places
    : gLibrary.ContentTree.view;
  tree.selectItems([item.guid]);
  let treeIndex = tree.view.treeIndexForNode(tree.selectedNode);
  let title = tree.view.getCellText(treeIndex, tree.columns.getColumnAt(0));
  if (useLeftPane) {
    // Make the treeIndex relative to the parent, otherwise getting the right
    // index value is tricky due to separators and URIs being hidden in the left
    // pane.
    treeIndex -= parentTreeIndex + 1;
  }
  return {
    node: tree.selectedNode,
    title,
    parentRelativeTreeIndex: treeIndex,
  };
}

function itemExists(item) {
  let useLeftPane =
    item.type == PlacesUtils.bookmarks.TYPE_FOLDER || // is Folder
    (item.type == PlacesUtils.bookmarks.TYPE_BOOKMARK &&
      item.url.protocol == "place:"); // is Query
  let tree = useLeftPane
    ? gLibrary.PlacesOrganizer._places
    : gLibrary.ContentTree.view;
  tree.selectItems([item.guid], true);
  return tree.selectedNode?.bookmarkGuid == item.guid;
}

async function insertAndCheckItem(insertItem, expectedParentRelativeIndex) {
  // Ensure the parent is selected before the change, this covers live updating
  // better than selecting the parent later, that would just refresh all its
  // children.
  Assert.ok(insertItem.parentGuid, "Must have a parentGuid");
  gLibrary.PlacesOrganizer._places.selectItems([insertItem.parentGuid], true);
  let tree = gLibrary.PlacesOrganizer._places;
  let parentTreeIndex = tree.view.treeIndexForNode(tree.selectedNode);

  let item = await PlacesUtils.bookmarks.insert(insertItem);

  let { node, title, parentRelativeTreeIndex } = selectItem(
    item,
    parentTreeIndex
  );
  Assert.equal(item.guid, node.bookmarkGuid, "Should find the updated node");
  Assert.equal(title, item.title, "Should have the correct title");
  Assert.equal(
    parentRelativeTreeIndex,
    expectedParentRelativeIndex,
    "Should have the expected index"
  );
  return item;
}

async function updateAndCheckItem(updateItem, expectedParentRelativeTreeIndex) {
  // Ensure the parent is selected before the change, this covers live updating
  // better than selecting the parent later, that would just refresh all its
  // children.
  Assert.ok(updateItem.parentGuid, "Must have a parentGuid");
  gLibrary.PlacesOrganizer._places.selectItems([updateItem.parentGuid], true);
  let tree = gLibrary.PlacesOrganizer._places;
  let parentTreeIndex = tree.view.treeIndexForNode(tree.selectedNode);

  let item = await PlacesUtils.bookmarks.update(updateItem);

  let { node, title, parentRelativeTreeIndex } = selectItem(
    item,
    parentTreeIndex
  );
  Assert.equal(item.guid, node.bookmarkGuid, "Should find the updated node");
  Assert.equal(title, item.title, "Should have the correct title");
  Assert.equal(
    parentRelativeTreeIndex,
    expectedParentRelativeTreeIndex,
    "Should have the expected index"
  );
  return item;
}

async function removeAndCheckItem(itemData) {
  await PlacesUtils.bookmarks.remove(itemData);
  Assert.ok(!itemExists(itemData), "Should not find the updated node");
}
