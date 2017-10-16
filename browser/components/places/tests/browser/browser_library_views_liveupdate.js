/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests Library Left pane view for liveupdate.
 */

let gLibrary = null;

async function testInFolder(folderGuid, prefix) {
  let addedBookmarks = [];
  let item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    title: `${prefix}1`,
    url: `http://${prefix}1.mozilla.org/`,
  });
  item.title = `${prefix}1_edited`;
  await PlacesUtils.bookmarks.update(item);
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    title: `${prefix}2`,
    url: "place:",
  });

  item.title = `${prefix}2_edited`;
  await PlacesUtils.bookmarks.update(item);
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    title: `${prefix}f`,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  item.title = `${prefix}f_edited`;
  await PlacesUtils.bookmarks.update(item);
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: item.guid,
    title: `${prefix}f1`,
    url: `http://${prefix}f1.mozilla.org/`,
  });
  addedBookmarks.push(item);

  item.index = 0;
  await PlacesUtils.bookmarks.update(item);

  return addedBookmarks;
}

add_task(async function test() {
  // This test takes quite some time, and timeouts frequently, so we require
  // more time to run.
  // See Bug 525610.
  requestLongerTimeout(2);

  // Open Library, we will check the left pane.
  gLibrary = await promiseLibrary();

  // Add observers.
  PlacesUtils.bookmarks.addObserver(bookmarksObserver);
  PlacesUtils.annotations.addObserver(bookmarksObserver);
  let addedBookmarks = [];

  // MENU
  ok(true, "*** Acting on menu bookmarks");
  addedBookmarks = addedBookmarks.concat(await testInFolder(PlacesUtils.bookmarks.menuGuid, "bm"));

  // TOOLBAR
  ok(true, "*** Acting on toolbar bookmarks");
  addedBookmarks = addedBookmarks.concat(await testInFolder(PlacesUtils.bookmarks.toolbarGuid, "tb"));

  // UNSORTED
  ok(true, "*** Acting on unsorted bookmarks");
  addedBookmarks = addedBookmarks.concat(await testInFolder(PlacesUtils.bookmarks.unfiledGuid, "ub"));

  // Remove all added bookmarks.
  for (let i = 0; i < addedBookmarks.length; i++) {
    // If we remove an item after its containing folder has been removed,
    // this will throw, but we can ignore that.
    try {
      await PlacesUtils.bookmarks.remove(addedBookmarks[i]);
    } catch (ex) {}
  }

  // Remove observers.
  PlacesUtils.bookmarks.removeObserver(bookmarksObserver);
  PlacesUtils.annotations.removeObserver(bookmarksObserver);

  await promiseLibraryClosed(gLibrary);
});

/**
 * The observer is where magic happens, for every change we do it will look for
 * nodes positions in the affected views.
 */
let bookmarksObserver = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsIAnnotationObserver
  ]),

  // nsIAnnotationObserver
  onItemAnnotationSet() {},
  onItemAnnotationRemoved() {},
  onPageAnnotationSet() {},
  onPageAnnotationRemoved() {},

  // nsINavBookmarkObserver
  onItemAdded: function PSB_onItemAdded(aItemId, aFolderId, aIndex, aItemType,
                                        aURI) {
    let node = null;
    let index = null;
    [node, index] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places);
    // Left pane should not be updated for normal bookmarks or separators.
    switch (aItemType) {
      case PlacesUtils.bookmarks.TYPE_BOOKMARK:
        let uriString = aURI.spec;
        let isQuery = uriString.substr(0, 6) == "place:";
        if (isQuery) {
          isnot(node, null, "Found new Places node in left pane");
          ok(index >= 0, "Node is at index " + index);
          break;
        }
        // Fallback to separator case if this is not a query.
      case PlacesUtils.bookmarks.TYPE_SEPARATOR:
        is(node, null, "New Places node not added in left pane");
        break;
      default:
        isnot(node, null, "Found new Places node in left pane");
        ok(index >= 0, "Node is at index " + index);
    }
  },

  onItemRemoved: function PSB_onItemRemoved(aItemId, aFolder, aIndex) {
    let node = null;
    [node, ] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places);
    is(node, null, "Places node not found in left pane");
  },

  onItemMoved(aItemId,
                        aOldFolderId, aOldIndex,
                        aNewFolderId, aNewIndex, aItemType) {
    let node = null;
    let index = null;
    [node, index] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places);
    // Left pane should not be updated for normal bookmarks or separators.
    switch (aItemType) {
      case PlacesUtils.bookmarks.TYPE_BOOKMARK:
        let uriString = PlacesUtils.bookmarks.getBookmarkURI(aItemId).spec;
        let isQuery = uriString.substr(0, 6) == "place:";
        if (isQuery) {
          isnot(node, null, "Found new Places node in left pane");
          ok(index >= 0, "Node is at index " + index);
          break;
        }
        // Fallback to separator case if this is not a query.
      case PlacesUtils.bookmarks.TYPE_SEPARATOR:
        is(node, null, "New Places node not added in left pane");
        break;
      default:
        isnot(node, null, "Found new Places node in left pane");
        ok(index >= 0, "Node is at index " + index);
    }
  },

  onBeginUpdateBatch: function PSB_onBeginUpdateBatch() {},
  onEndUpdateBatch: function PSB_onEndUpdateBatch() {},
  onItemVisited() {},
  onItemChanged: function PSB_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aNewValue) {
    if (aProperty == "title") {
      let validator = function(aTreeRowIndex) {
        let tree = gLibrary.PlacesOrganizer._places;
        let cellText = tree.view.getCellText(aTreeRowIndex,
                                             tree.columns.getColumnAt(0));
        return cellText == aNewValue;
      };
      let [node, , valid] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places, validator);
      if (node) // Only visible nodes.
        ok(valid, "Title cell value has been correctly updated");
    }
  }
};


/**
 * Get places node and index for an itemId in a tree view.
 *
 * @param aItemId
 *        item id of the item to search.
 * @param aTree
 *        Tree to search in.
 * @param aValidator [optional]
 *        function to check row validity if found.  Defaults to {return true;}.
 * @returns [node, index, valid] or [null, null, false] if not found.
 */
function getNodeForTreeItem(aItemId, aTree, aValidator) {

  function findNode(aContainerIndex) {
    if (aTree.view.isContainerEmpty(aContainerIndex))
      return [null, null, false];

    // The rowCount limit is just for sanity, but we will end looping when
    // we have checked the last child of this container or we have found node.
    for (let i = aContainerIndex + 1; i < aTree.view.rowCount; i++) {
      let node = aTree.view.nodeForTreeIndex(i);

      if (node.itemId == aItemId) {
        // Minus one because we want relative index inside the container.
        let valid = aValidator ? aValidator(i) : true;
        return [node, i - aTree.view.getParentIndex(i) - 1, valid];
      }

      if (PlacesUtils.nodeIsFolder(node)) {
        // Open container.
        aTree.view.toggleOpenState(i);
        // Search inside it.
        let foundNode = findNode(i);
        // Close container.
        aTree.view.toggleOpenState(i);
        // Return node if found.
        if (foundNode[0] != null)
          return foundNode;
      }

      // We have finished walking this container.
      if (!aTree.view.hasNextSibling(aContainerIndex + 1, i))
        break;
    }
    return [null, null, false];
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
    if (foundNode[0] != null)
      return foundNode;
  }
  return [null, null, false];
}
