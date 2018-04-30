/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests Places views (menu, toolbar, tree) for liveupdate.
 */

var toolbar = document.getElementById("PersonalToolbar");
var wasCollapsed = toolbar.collapsed;

/**
 * Simulates popup opening causing it to populate.
 * We cannot just use menu.open, since it would not work on Mac due to native menubar.
 */
function fakeOpenPopup(aPopup) {
  var popupEvent = document.createEvent("MouseEvent");
  popupEvent.initMouseEvent("popupshowing", true, true, window, 0,
                            0, 0, 0, 0, false, false, false, false,
                            0, null);
  aPopup.dispatchEvent(popupEvent);
}

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
  item.parentGuid = folderGuid;
  await PlacesUtils.bookmarks.update(item);

  return addedBookmarks;
}

add_task(async function test() {
  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  // Open bookmarks menu.
  var popup = document.getElementById("bookmarksMenuPopup");
  ok(popup, "Menu popup element exists");
  fakeOpenPopup(popup);

  // Open bookmarks sidebar.
  await withSidebarTree("bookmarks", async () => {
    // Add observers.
    PlacesUtils.bookmarks.addObserver(bookmarksObserver);
    PlacesUtils.annotations.addObserver(bookmarksObserver);
    var addedBookmarks = [];

    // MENU
    info("*** Acting on menu bookmarks");
    addedBookmarks = addedBookmarks.concat(await testInFolder(PlacesUtils.bookmarks.menuGuid, "bm"));

    // TOOLBAR
    info("*** Acting on toolbar bookmarks");
    addedBookmarks = addedBookmarks.concat(await testInFolder(PlacesUtils.bookmarks.toolbarGuid, "tb"));

    // UNSORTED
    info("*** Acting on unsorted bookmarks");
    addedBookmarks = addedBookmarks.concat(await testInFolder(PlacesUtils.bookmarks.unfiledGuid, "ub"));

    // Remove all added bookmarks.
    for (let bm of addedBookmarks) {
      // If we remove an item after its containing folder has been removed,
      // this will throw, but we can ignore that.
      try {
        await PlacesUtils.bookmarks.remove(bm);
      } catch (ex) {}
    }

    // Remove observers.
    PlacesUtils.bookmarks.removeObserver(bookmarksObserver);
    PlacesUtils.annotations.removeObserver(bookmarksObserver);
  });

  // Collapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, false);
  }
});

/**
 * The observer is where magic happens, for every change we do it will look for
 * nodes positions in the affected views.
 */
var bookmarksObserver = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsIAnnotationObserver
  ]),

  // nsIAnnotationObserver
  onItemAnnotationSet() {},
  onItemAnnotationRemoved() {},
  onPageAnnotationSet() {},
  onPageAnnotationRemoved() {},

  // nsINavBookmarkObserver
  onItemAdded: function PSB_onItemAdded(aItemId, aFolderId, aIndex,
                                        aItemType, aURI) {
    var views = getViewsForFolder(aFolderId);
    ok(views.length > 0, "Found affected views (" + views.length + "): " + views);

    // Check that item has been added in the correct position.
    for (var i = 0; i < views.length; i++) {
      var [node, index] = searchItemInView(aItemId, views[i]);
      isnot(node, null, "Found new Places node in " + views[i]);
      is(index, aIndex, "Node is at index " + index);
    }
  },

  onItemRemoved: function PSB_onItemRemoved(aItemId, aFolderId, aIndex,
                                            aItemType, url, aGuid) {
    var views = getViewsForFolder(aFolderId);
    ok(views.length > 0, "Found affected views (" + views.length + "): " + views);
    // Check that item has been removed.
    for (var i = 0; i < views.length; i++) {
      var node = null;
      [node, ] = searchItemInView(aItemId, views[i]);
      is(node, null, "Places node should not be found in " + views[i]);
    }
  },

  onItemMoved(aItemId,
                        aOldFolderId, aOldIndex,
                        aNewFolderId, aNewIndex,
                        aItemType) {
    var views = getViewsForFolder(aNewFolderId);
    ok(views.length > 0, "Found affected views: " + views);

    // Check that item has been moved in the correct position.
    for (var i = 0; i < views.length; i++) {
      var node = null;
      var index = null;
      [node, index] = searchItemInView(aItemId, views[i]);
      isnot(node, null, "Found new Places node in " + views[i]);
      is(index, aNewIndex, "Node is at index " + index);
    }
  },

  onBeginUpdateBatch: function PSB_onBeginUpdateBatch() {},
  onEndUpdateBatch: function PSB_onEndUpdateBatch() {},
  onItemVisited() {},

  onItemChanged: function PSB_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aNewValue,
                                            aLastModified, aItemType,
                                            aParentId) {
    if (aProperty !== "title")
      return;

    var views = getViewsForFolder(aParentId);
    ok(views.length > 0, "Found affected views (" + views.length + "): " + views);

    // Check that item has been moved in the correct position.
    let validator = function(aElementOrTreeIndex) {
      if (typeof(aElementOrTreeIndex) == "number") {
        var sidebar = document.getElementById("sidebar");
        var tree = sidebar.contentDocument.getElementById("bookmarks-view");
        let cellText = tree.view.getCellText(aElementOrTreeIndex,
                                             tree.columns.getColumnAt(0));
        if (!aNewValue)
          return cellText == PlacesUIUtils.getBestTitle(tree.view.nodeForTreeIndex(aElementOrTreeIndex), true);
        return cellText == aNewValue;
      }
      if (!aNewValue && aElementOrTreeIndex.localName != "toolbarbutton") {
        return aElementOrTreeIndex.getAttribute("label") == PlacesUIUtils.getBestTitle(aElementOrTreeIndex._placesNode);
      }
      return aElementOrTreeIndex.getAttribute("label") == aNewValue;
    };

    for (var i = 0; i < views.length; i++) {
      var [node, , valid] = searchItemInView(aItemId, views[i], validator);
      isnot(node, null, "Found changed Places node in " + views[i]);
      is(node.title, aNewValue, "Node has correct title: " + aNewValue);
      ok(valid, "Node element has correct label: " + aNewValue);
    }
  }
};

/**
 * Search an item id in a view.
 *
 * @param aItemId
 *        item id of the item to search.
 * @param aView
 *        either "toolbar", "menu" or "sidebar"
 * @param aValidator
 *        function to check validity of the found node element.
 * @returns [node, index, valid] or [null, null, false] if not found.
 */
function searchItemInView(aItemId, aView, aValidator) {
  switch (aView) {
  case "toolbar":
    return getNodeForToolbarItem(aItemId, aValidator);
  case "menu":
    return getNodeForMenuItem(aItemId, aValidator);
  case "sidebar":
    return getNodeForSidebarItem(aItemId, aValidator);
  }

  return [null, null, false];
}

/**
 * Get places node and index for an itemId in bookmarks toolbar view.
 *
 * @param aItemId
 *        item id of the item to search.
 * @returns [node, index] or [null, null] if not found.
 */
function getNodeForToolbarItem(aItemId, aValidator) {
  var placesToolbarItems = document.getElementById("PlacesToolbarItems");

  function findNode(aContainer) {
    var children = aContainer.childNodes;
    for (var i = 0, staticNodes = 0; i < children.length; i++) {
      var child = children[i];

      // Is this a Places node?
      if (!child._placesNode) {
        staticNodes++;
        continue;
      }

      if (child._placesNode.itemId == aItemId) {
        let valid = aValidator ? aValidator(child) : true;
        return [child._placesNode, i - staticNodes, valid];
      }

      // Don't search in queries, they could contain our item in a
      // different position.  Search only folders
      if (PlacesUtils.nodeIsFolder(child._placesNode)) {
        var popup = child.lastChild;
        popup.openPopup();
        var foundNode = findNode(popup);
        popup.hidePopup();
        if (foundNode[0] != null)
          return foundNode;
      }
    }
    return [null, null];
  }

  return findNode(placesToolbarItems);
}

/**
 * Get places node and index for an itemId in bookmarks menu view.
 *
 * @param aItemId
 *        item id of the item to search.
 * @returns [node, index] or [null, null] if not found.
 */
function getNodeForMenuItem(aItemId, aValidator) {
  var menu = document.getElementById("bookmarksMenu");

  function findNode(aContainer) {
    var children = aContainer.childNodes;
    for (var i = 0, staticNodes = 0; i < children.length; i++) {
      var child = children[i];

      // Is this a Places node?
      if (!child._placesNode) {
        staticNodes++;
        continue;
      }

      if (child._placesNode.itemId == aItemId) {
        let valid = aValidator ? aValidator(child) : true;
        return [child._placesNode, i - staticNodes, valid];
      }

      // Don't search in queries, they could contain our item in a
      // different position.  Search only folders
      if (PlacesUtils.nodeIsFolder(child._placesNode)) {
        var popup = child.lastChild;
        fakeOpenPopup(popup);
        var foundNode = findNode(popup);

        child.open = false;
        if (foundNode[0] != null)
          return foundNode;
      }
    }
    return [null, null, false];
  }

  return findNode(menu.lastChild);
}

/**
 * Get places node and index for an itemId in sidebar tree view.
 *
 * @param aItemId
 *        item id of the item to search.
 * @returns [node, index] or [null, null] if not found.
 */
function getNodeForSidebarItem(aItemId, aValidator) {
  var sidebar = document.getElementById("sidebar");
  var tree = sidebar.contentDocument.getElementById("bookmarks-view");

  function findNode(aContainerIndex) {
    if (tree.view.isContainerEmpty(aContainerIndex))
      return [null, null, false];

    // The rowCount limit is just for sanity, but we will end looping when
    // we have checked the last child of this container or we have found node.
    for (var i = aContainerIndex + 1; i < tree.view.rowCount; i++) {
      var node = tree.view.nodeForTreeIndex(i);

      if (node.itemId == aItemId) {
        // Minus one because we want relative index inside the container.
        let valid = aValidator ? aValidator(i) : true;
        return [node, i - tree.view.getParentIndex(i) - 1, valid];
      }

      if (PlacesUtils.nodeIsFolder(node)) {
        // Open container.
        tree.view.toggleOpenState(i);
        // Search inside it.
        var foundNode = findNode(i);
        // Close container.
        tree.view.toggleOpenState(i);
        // Return node if found.
        if (foundNode[0] != null)
          return foundNode;
      }

      // We have finished walking this container.
      if (!tree.view.hasNextSibling(aContainerIndex + 1, i))
        break;
    }
    return [null, null, false];
  }

  // Root node is hidden, so we need to manually walk the first level.
  for (var i = 0; i < tree.view.rowCount; i++) {
    // Open container.
    tree.view.toggleOpenState(i);
    // Search inside it.
    var foundNode = findNode(i);
    // Close container.
    tree.view.toggleOpenState(i);
    // Return node if found.
    if (foundNode[0] != null)
      return foundNode;
  }
  return [null, null, false];
}

/**
 * Get views affected by changes to a folder.
 *
 * @param aFolderId:
 *        item id of the folder we have changed.
 * @returns a subset of views: ["toolbar", "menu", "sidebar"]
 */
function getViewsForFolder(aFolderId) {
  var rootId = aFolderId;
  while (!PlacesUtils.isRootItem(rootId))
    rootId = PlacesUtils.bookmarks.getFolderIdForItem(rootId);

  switch (rootId) {
    case PlacesUtils.toolbarFolderId:
      return ["toolbar", "sidebar"];
    case PlacesUtils.bookmarksMenuFolderId:
      return ["menu", "sidebar"];
    case PlacesUtils.unfiledBookmarksFolderId:
      return ["sidebar"];
  }
  return [];
}
