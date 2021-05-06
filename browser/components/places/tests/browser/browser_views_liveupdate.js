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
  popupEvent.initMouseEvent(
    "popupshowing",
    true,
    true,
    window,
    0,
    0,
    0,
    0,
    0,
    false,
    false,
    false,
    false,
    0,
    null
  );
  aPopup.dispatchEvent(popupEvent);
}

async function testInFolder(folderGuid, prefix) {
  let addedBookmarks = [];
  let item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    title: `${prefix}1`,
    url: `http://${prefix}1.mozilla.org/`,
  });
  await bookmarksObserver.assertViewsUpdatedCorrectly();

  item.title = `${prefix}1_edited`;
  await PlacesUtils.bookmarks.update(item);
  await bookmarksObserver.assertViewsUpdatedCorrectly();

  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    title: `${prefix}2`,
    url: "place:",
  });
  await bookmarksObserver.assertViewsUpdatedCorrectly();

  item.title = `${prefix}2_edited`;
  await PlacesUtils.bookmarks.update(item);
  await bookmarksObserver.assertViewsUpdatedCorrectly();
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });
  await bookmarksObserver.assertViewsUpdatedCorrectly();
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: folderGuid,
    title: `${prefix}f`,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  await bookmarksObserver.assertViewsUpdatedCorrectly();

  item.title = `${prefix}f_edited`;
  await PlacesUtils.bookmarks.update(item);
  await bookmarksObserver.assertViewsUpdatedCorrectly();
  addedBookmarks.push(item);

  item = await PlacesUtils.bookmarks.insert({
    parentGuid: item.guid,
    title: `${prefix}f1`,
    url: `http://${prefix}f1.mozilla.org/`,
  });
  await bookmarksObserver.assertViewsUpdatedCorrectly();
  addedBookmarks.push(item);

  item.index = 0;
  item.parentGuid = folderGuid;
  await PlacesUtils.bookmarks.update(item);
  await bookmarksObserver.assertViewsUpdatedCorrectly();

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
    bookmarksObserver.handlePlacesEvents = bookmarksObserver.handlePlacesEvents.bind(
      bookmarksObserver
    );
    PlacesUtils.bookmarks.addObserver(bookmarksObserver);
    PlacesUtils.observers.addListener(
      ["bookmark-added", "bookmark-removed"],
      bookmarksObserver.handlePlacesEvents
    );
    var addedBookmarks = [];

    // MENU
    info("*** Acting on menu bookmarks");
    addedBookmarks = addedBookmarks.concat(
      await testInFolder(PlacesUtils.bookmarks.menuGuid, "bm")
    );

    // TOOLBAR
    info("*** Acting on toolbar bookmarks");
    addedBookmarks = addedBookmarks.concat(
      await testInFolder(PlacesUtils.bookmarks.toolbarGuid, "tb")
    );

    // UNSORTED
    info("*** Acting on unsorted bookmarks");
    addedBookmarks = addedBookmarks.concat(
      await testInFolder(PlacesUtils.bookmarks.unfiledGuid, "ub")
    );

    // Remove all added bookmarks.
    for (let bm of addedBookmarks) {
      // If we remove an item after its containing folder has been removed,
      // this will throw, but we can ignore that.
      try {
        await PlacesUtils.bookmarks.remove(bm);
      } catch (ex) {}
      await bookmarksObserver.assertViewsUpdatedCorrectly();
    }

    // Remove observers.
    PlacesUtils.bookmarks.removeObserver(bookmarksObserver);
    PlacesUtils.observers.removeListener(
      ["bookmark-added", "bookmark-removed"],
      bookmarksObserver.handlePlacesEvents
    );
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
  _notifications: [],

  QueryInterface: ChromeUtils.generateQI(["nsINavBookmarkObserver"]),

  handlePlacesEvents(events) {
    for (let { type, parentGuid, guid, index } of events) {
      switch (type) {
        case "bookmark-added":
          this._notifications.push([
            "assertItemAdded",
            parentGuid,
            guid,
            index,
          ]);
          break;
        case "bookmark-removed":
          this._notifications.push(["assertItemRemoved", parentGuid, guid]);
          break;
      }
    }
  },

  onItemChanged(
    itemId,
    property,
    annoProperty,
    newValue,
    lastModified,
    itemType,
    parentId,
    guid,
    parentGuid
  ) {
    if (property !== "title") {
      return;
    }

    this._notifications.push(["assertItemChanged", parentGuid, guid, newValue]);
  },

  async assertViewsUpdatedCorrectly() {
    for (let notification of this._notifications) {
      let assertFunction = notification.shift();

      let views = await getViewsForFolder(notification.shift());
      Assert.greater(
        views.length,
        0,
        "Should have found one or more views for the parent folder."
      );

      await this[assertFunction](views, ...notification);
    }

    this._notifications = [];
  },

  async assertItemAdded(views, guid, expectedIndex) {
    for (let i = 0; i < views.length; i++) {
      let [node, index] = searchItemInView(guid, views[i]);
      Assert.notEqual(node, null, "Should have found the view in " + views[i]);
      Assert.equal(
        index,
        expectedIndex,
        "Should have found the node at the expected index"
      );
    }
  },

  async assertItemRemoved(views, guid) {
    for (let i = 0; i < views.length; i++) {
      let [node] = searchItemInView(guid, views[i]);
      Assert.equal(node, null, "Should not have found the node");
    }
  },

  async assertItemMoved(views, guid, newIndex) {
    // Check that item has been moved in the correct position.
    for (let i = 0; i < views.length; i++) {
      let [node, index] = searchItemInView(guid, views[i]);
      Assert.notEqual(node, null, "Should have found the view in " + views[i]);
      Assert.equal(
        index,
        newIndex,
        "Should have found the node at the expected index"
      );
    }
  },

  async assertItemChanged(views, guid, newValue) {
    let validator = function(aElementOrTreeIndex) {
      if (typeof aElementOrTreeIndex == "number") {
        let sidebar = document.getElementById("sidebar");
        let tree = sidebar.contentDocument.getElementById("bookmarks-view");
        let cellText = tree.view.getCellText(
          aElementOrTreeIndex,
          tree.columns.getColumnAt(0)
        );
        if (!newValue) {
          return (
            cellText ==
            PlacesUIUtils.getBestTitle(
              tree.view.nodeForTreeIndex(aElementOrTreeIndex),
              true
            )
          );
        }
        return cellText == newValue;
      }
      if (!newValue && aElementOrTreeIndex.localName != "toolbarbutton") {
        return (
          aElementOrTreeIndex.getAttribute("label") ==
          PlacesUIUtils.getBestTitle(aElementOrTreeIndex._placesNode)
        );
      }
      return aElementOrTreeIndex.getAttribute("label") == newValue;
    };

    for (let i = 0; i < views.length; i++) {
      let [node, , valid] = searchItemInView(guid, views[i], validator);
      Assert.notEqual(node, null, "Should have found the view in " + views[i]);
      Assert.equal(
        node.title,
        newValue,
        "Node should have the correct new title"
      );
      Assert.ok(valid, "Node element should have the correct label");
    }
  },
};

/**
 * Search an item guid in a view.
 *
 * @param itemGuid
 *        item guid of the item to search.
 * @param view
 *        either "toolbar", "menu" or "sidebar"
 * @param validator
 *        function to check validity of the found node element.
 * @returns [node, index, valid] or [null, null, false] if not found.
 */
function searchItemInView(itemGuid, view, validator) {
  switch (view) {
    case "toolbar":
      return getNodeForToolbarItem(itemGuid, validator);
    case "menu":
      return getNodeForMenuItem(itemGuid, validator);
    case "sidebar":
      return getNodeForSidebarItem(itemGuid, validator);
  }

  return [null, null, false];
}

/**
 * Get places node and index for an itemGuid in bookmarks toolbar view.
 *
 * @param itemGuid
 *        item guid of the item to search.
 * @returns [node, index] or [null, null] if not found.
 */
function getNodeForToolbarItem(itemGuid, validator) {
  var placesToolbarItems = document.getElementById("PlacesToolbarItems");

  function findNode(aContainer) {
    var children = aContainer.children;
    for (var i = 0, staticNodes = 0; i < children.length; i++) {
      var child = children[i];

      // Is this a Places node?
      if (!child._placesNode) {
        staticNodes++;
        continue;
      }

      if (child._placesNode.bookmarkGuid == itemGuid) {
        let valid = validator ? validator(child) : true;
        return [child._placesNode, i - staticNodes, valid];
      }

      // Don't search in queries, they could contain our item in a
      // different position.  Search only folders
      if (PlacesUtils.nodeIsFolder(child._placesNode)) {
        var popup = child.menupopup;
        popup.openPopup();
        var foundNode = findNode(popup);
        popup.hidePopup();
        if (foundNode[0] != null) {
          return foundNode;
        }
      }
    }
    return [null, null];
  }

  return findNode(placesToolbarItems);
}

/**
 * Get places node and index for an itemGuid in bookmarks menu view.
 *
 * @param itemGuid
 *        item guid of the item to search.
 * @returns [node, index] or [null, null] if not found.
 */
function getNodeForMenuItem(itemGuid, validator) {
  var menu = document.getElementById("bookmarksMenu");

  function findNode(aContainer) {
    var children = aContainer.children;
    for (var i = 0, staticNodes = 0; i < children.length; i++) {
      var child = children[i];

      // Is this a Places node?
      if (!child._placesNode) {
        staticNodes++;
        continue;
      }

      if (child._placesNode.bookmarkGuid == itemGuid) {
        let valid = validator ? validator(child) : true;
        return [child._placesNode, i - staticNodes, valid];
      }

      // Don't search in queries, they could contain our item in a
      // different position.  Search only folders
      if (PlacesUtils.nodeIsFolder(child._placesNode)) {
        var popup = child.lastElementChild;
        fakeOpenPopup(popup);
        var foundNode = findNode(popup);

        child.open = false;
        if (foundNode[0] != null) {
          return foundNode;
        }
      }
    }
    return [null, null, false];
  }

  return findNode(menu.lastElementChild);
}

/**
 * Get places node and index for an itemGuid in sidebar tree view.
 *
 * @param itemGuid
 *        item guid of the item to search.
 * @returns [node, index] or [null, null] if not found.
 */
function getNodeForSidebarItem(itemGuid, validator) {
  var sidebar = document.getElementById("sidebar");
  var tree = sidebar.contentDocument.getElementById("bookmarks-view");

  function findNode(aContainerIndex) {
    if (tree.view.isContainerEmpty(aContainerIndex)) {
      return [null, null, false];
    }

    // The rowCount limit is just for sanity, but we will end looping when
    // we have checked the last child of this container or we have found node.
    for (var i = aContainerIndex + 1; i < tree.view.rowCount; i++) {
      var node = tree.view.nodeForTreeIndex(i);

      if (node.bookmarkGuid == itemGuid) {
        // Minus one because we want relative index inside the container.
        let valid = validator ? validator(i) : true;
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
        if (foundNode[0] != null) {
          return foundNode;
        }
      }

      // We have finished walking this container.
      if (!tree.view.hasNextSibling(aContainerIndex + 1, i)) {
        break;
      }
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
    if (foundNode[0] != null) {
      return foundNode;
    }
  }
  return [null, null, false];
}

/**
 * Get views affected by changes to a folder.
 *
 * @param folderGuid:
 *        item guid of the folder we have changed.
 * @returns a subset of views: ["toolbar", "menu", "sidebar"]
 */
async function getViewsForFolder(folderGuid) {
  let rootGuid = folderGuid;
  while (!PlacesUtils.isRootItem(rootGuid)) {
    let itemData = await PlacesUtils.bookmarks.fetch(rootGuid);
    rootGuid = itemData.parentGuid;
  }

  switch (rootGuid) {
    case PlacesUtils.bookmarks.toolbarGuid:
      return ["toolbar", "sidebar"];
    case PlacesUtils.bookmarks.menuGuid:
      return ["menu", "sidebar"];
    case PlacesUtils.bookmarks.unfiledGuid:
      return ["sidebar"];
  }
  return [];
}
