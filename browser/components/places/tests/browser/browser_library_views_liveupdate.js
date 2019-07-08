/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests Library Left pane view for liveupdate.
 */

let gLibrary = null;

add_task(async function setup() {
  gLibrary = await promiseLibrary();

  await PlacesUtils.bookmarks.eraseEverything();

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    await promiseLibraryClosed(gLibrary);
  });
});

async function testInFolder(folderGuid, prefix) {
  let addedBookmarks = [];

  let item = await insertAndCheckItem({
    parentGuid: folderGuid,
    title: `${prefix}1`,
    url: `http://${prefix}1.mozilla.org/`,
  });
  item.title = `${prefix}1_edited`;
  await updateAndCheckItem(item);
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

  item = await insertAndCheckItem({
    parentGuid: folderGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });
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

  item = await insertAndCheckItem({
    parentGuid: folderGuid1,
    title: `${prefix}f1`,
    url: `http://${prefix}f1.mozilla.org/`,
  });
  addedBookmarks.push(item);

  item = await insertAndCheckItem({
    parentGuid: folderGuid1,
    title: `${prefix}f12`,
    url: `http://${prefix}f12.mozilla.org/`,
  });
  addedBookmarks.push(item);

  item.index = 0;
  await updateAndCheckItem(item);

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

async function insertAndCheckItem(itemData, expectedIndex) {
  let item = await PlacesUtils.bookmarks.insert(itemData);

  let [node, index, title] = getNodeForTreeItem(
    item.guid,
    gLibrary.PlacesOrganizer._places
  );
  // Left pane should not be updated for normal bookmarks or separators.
  switch (itemData.type || PlacesUtils.bookmarks.TYPE_BOOKMARK) {
    case PlacesUtils.bookmarks.TYPE_BOOKMARK:
      let uriString = itemData.url;
      let isQuery = uriString.substr(0, 6) == "place:";
      if (isQuery) {
        Assert.ok(node, "Should have a new query in the left pane.");
        break;
      }
    // Fallthrough if this isn't a query
    case PlacesUtils.bookmarks.TYPE_SEPARATOR:
      Assert.ok(
        !node,
        "Should not have added a bookmark or separator to the left pane."
      );
      break;
    default:
      Assert.ok(
        node,
        "Should have added a new node in the left pane for a folder."
      );
  }

  if (node) {
    Assert.equal(title, itemData.title, "Should have the correct title");
    Assert.equal(index, expectedIndex, "Should have the expected index");
  }

  return item;
}

async function updateAndCheckItem(newItemData, expectedIndex) {
  await PlacesUtils.bookmarks.update(newItemData);

  let [node, index, title] = getNodeForTreeItem(
    newItemData.guid,
    gLibrary.PlacesOrganizer._places
  );

  // Left pane should not be updated for normal bookmarks or separators.
  switch (newItemData.type || PlacesUtils.bookmarks.TYPE_BOOKMARK) {
    case PlacesUtils.bookmarks.TYPE_BOOKMARK:
      let isQuery = newItemData.url.protocol == "place:";
      if (isQuery) {
        Assert.ok(node, "Should be able to find the updated node");
        break;
      }
    // Fallthrough if this isn't a query
    case PlacesUtils.bookmarks.TYPE_SEPARATOR:
      Assert.ok(!node, "Should not be able to find the updated node");
      break;
    default:
      Assert.ok(node, "Should be able to find the updated node");
  }

  if (node) {
    Assert.equal(title, newItemData.title, "Should have the correct title");
    Assert.equal(index, expectedIndex, "Should have the expected index");
  }
}

async function removeAndCheckItem(itemData) {
  await PlacesUtils.bookmarks.remove(itemData);
  let [node] = getNodeForTreeItem(
    itemData.guid,
    gLibrary.PlacesOrganizer._places
  );
  Assert.ok(!node, "Should not be able to find the removed node");
}

/**
 * Get places node, index and cell text for a guid in a tree view.
 *
 * @param aItemGuid
 *        item guid of the item to search.
 * @param aTree
 *        Tree to search in.
 * @returns [node, index, cellText] or [null, null, ""] if not found.
 */
function getNodeForTreeItem(aItemGuid, aTree) {
  function findNode(aContainerIndex) {
    if (aTree.view.isContainerEmpty(aContainerIndex)) {
      return [null, null, ""];
    }

    // The rowCount limit is just for sanity, but we will end looping when
    // we have checked the last child of this container or we have found node.
    for (let i = aContainerIndex + 1; i < aTree.view.rowCount; i++) {
      let node = aTree.view.nodeForTreeIndex(i);

      if (node.bookmarkGuid == aItemGuid) {
        // Minus one because we want relative index inside the container.
        let tree = gLibrary.PlacesOrganizer._places;
        let cellText = tree.view.getCellText(i, tree.columns.getColumnAt(0));
        return [node, i - aContainerIndex - 1, cellText];
      }

      if (PlacesUtils.nodeIsFolder(node)) {
        // Open container.
        aTree.view.toggleOpenState(i);
        // Search inside it.
        let foundNode = findNode(i);
        // Close container.
        aTree.view.toggleOpenState(i);
        // Return node if found.
        if (foundNode[0] != null) {
          return foundNode;
        }
      }

      // We have finished walking this container.
      if (!aTree.view.hasNextSibling(aContainerIndex + 1, i)) {
        break;
      }
    }
    return [null, null, ""];
  }

  // Root node is hidden, so we need to manually walk the first level.
  for (let i = 0; i < aTree.view.rowCount; i++) {
    // Open container.
    aTree.view.toggleOpenState(i);
    // Search inside it.
    let foundNode = findNode(i);
    // Close container.
    aTree.view.toggleOpenState(i);
    // Return node if found.
    if (foundNode[0] != null) {
      return foundNode;
    }
  }
  return [null, null, ""];
}
