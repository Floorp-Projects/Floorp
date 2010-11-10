/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Tests Places views (menu, toolbar, tree) for liveupdate.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

let toolbar = document.getElementById("PersonalToolbar");
let wasCollapsed = toolbar.collapsed;

function test() {
  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed)
    setToolbarVisibility(toolbar, true);

  waitForExplicitFinish();

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in context");
  ok(PlacesUIUtils, "PlacesUIUtils in context");

  // Open bookmarks menu.
  var popup = document.getElementById("bookmarksMenuPopup");
  ok(popup, "Menu popup element exists");
  fakeOpenPopup(popup);

  // Open bookmarks sidebar.
  var sidebar = document.getElementById("sidebar");
  sidebar.addEventListener("load", function() {
    sidebar.removeEventListener("load", arguments.callee, true);
    // Need to executeSoon since the tree is initialized on sidebar load.
    executeSoon(startTest);
  }, true);
  toggleSidebar("viewBookmarksSidebar", true);
}

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

/**
 * Adds bookmarks observer, and executes a bunch of bookmarks operations.
 */
function startTest() {
  var bs = PlacesUtils.bookmarks;
  // Add bookmarks observer.
  bs.addObserver(bookmarksObserver, false);
  var addedBookmarks = [];

  // MENU
  info("*** Acting on menu bookmarks");
  var id = bs.insertBookmark(bs.bookmarksMenuFolder,
                             PlacesUtils._uri("http://bm1.mozilla.org/"),
                             bs.DEFAULT_INDEX,
                             "bm1");
  bs.setItemTitle(id, "bm1_edited");
  addedBookmarks.push(id);
  id = bs.insertBookmark(bs.bookmarksMenuFolder,
                         PlacesUtils._uri("place:"),
                         bs.DEFAULT_INDEX,
                         "bm2");
  bs.setItemTitle(id, "");
  addedBookmarks.push(id);
  id = bs.insertSeparator(bs.bookmarksMenuFolder, bs.DEFAULT_INDEX);
  addedBookmarks.push(id);
  id = bs.createFolder(bs.bookmarksMenuFolder,
                       "bmf",
                       bs.DEFAULT_INDEX);
  bs.setItemTitle(id, "bmf_edited");
  addedBookmarks.push(id);
  id = bs.insertBookmark(id,
                         PlacesUtils._uri("http://bmf1.mozilla.org/"),
                         bs.DEFAULT_INDEX,
                         "bmf1");
  bs.setItemTitle(id, "bmf1_edited");
  addedBookmarks.push(id);
  bs.moveItem(id, bs.bookmarksMenuFolder, 0);

  // TOOLBAR
  info("*** Acting on toolbar bookmarks");
  id = bs.insertBookmark(bs.toolbarFolder,
                         PlacesUtils._uri("http://tb1.mozilla.org/"),
                         bs.DEFAULT_INDEX,
                         "tb1");
  bs.setItemTitle(id, "tb1_edited");
  addedBookmarks.push(id);
  // Test live update of title.
  bs.setItemTitle(id, "tb1_edited");
  id = bs.insertBookmark(bs.toolbarFolder,
                         PlacesUtils._uri("place:"),
                         bs.DEFAULT_INDEX,
                         "tb2");
  bs.setItemTitle(id, "");
  addedBookmarks.push(id);
  id = bs.insertSeparator(bs.toolbarFolder, bs.DEFAULT_INDEX);
  addedBookmarks.push(id);
  id = bs.createFolder(bs.toolbarFolder,
                       "tbf",
                       bs.DEFAULT_INDEX);
  bs.setItemTitle(id, "tbf_edited");
  addedBookmarks.push(id);
  id = bs.insertBookmark(id,
                         PlacesUtils._uri("http://tbf1.mozilla.org/"),
                         bs.DEFAULT_INDEX,
                         "tbf1");
  bs.setItemTitle(id, "tbf1_edited");
  addedBookmarks.push(id);
  bs.moveItem(id, bs.toolbarFolder, 0);

  // UNSORTED
  info("*** Acting on unsorted bookmarks");
  id = bs.insertBookmark(bs.unfiledBookmarksFolder,
                         PlacesUtils._uri("http://ub1.mozilla.org/"),
                         bs.DEFAULT_INDEX,
                         "ub1");
  bs.setItemTitle(id, "ub1_edited");
  addedBookmarks.push(id);
  id = bs.insertBookmark(bs.unfiledBookmarksFolder,
                         PlacesUtils._uri("place:"),
                         bs.DEFAULT_INDEX,
                         "ub2");
  bs.setItemTitle(id, "ub2_edited");
  addedBookmarks.push(id);
  id = bs.insertSeparator(bs.unfiledBookmarksFolder, bs.DEFAULT_INDEX);
  addedBookmarks.push(id);
  id = bs.createFolder(bs.unfiledBookmarksFolder,
                       "ubf",
                       bs.DEFAULT_INDEX);
  bs.setItemTitle(id, "ubf_edited");
  addedBookmarks.push(id);
  id = bs.insertBookmark(id,
                         PlacesUtils._uri("http://ubf1.mozilla.org/"),
                         bs.DEFAULT_INDEX,
                         "bubf1");
  bs.setItemTitle(id, "bubf1_edited");
  addedBookmarks.push(id);
  bs.moveItem(id, bs.unfiledBookmarksFolder, 0);

  // Remove all added bookmarks.
  addedBookmarks.forEach(function (aItem) {
    // If we remove an item after its containing folder has been removed,
    // this will throw, but we can ignore that.
    try {
      bs.removeItem(aItem);
    } catch (ex) {}
  });

  // Remove bookmarks observer.
  bs.removeObserver(bookmarksObserver);
  finishTest();
}

/**
 * Restores browser state and calls finish.
 */
function finishTest() {
  // Close bookmarks sidebar.
  toggleSidebar("viewBookmarksSidebar", false);

  // Collapse the personal toolbar if needed.
  if (wasCollapsed)
    setToolbarVisibility(toolbar, false);

  finish();
}

/**
 * The observer is where magic happens, for every change we do it will look for
 * nodes positions in the affected views.
 */
var bookmarksObserver = {
  QueryInterface: function PSB_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsINavBookmarkObserver) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  },

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

  onItemRemoved: function PSB_onItemRemoved(aItemId, aFolder, aIndex,
                                            aItemType) {
    var views = getViewsForFolder(aFolderId);
    ok(views.length > 0, "Found affected views (" + views.length + "): " + views);
    // Check that item has been removed.
    for (var i = 0; i < views.length; i++) {
      var node = null;
      var index = null;
      [node, index] = searchItemInView(aItemId, views[i]);
      is(node, null, "Places node not found in " + views[i]);
    }
  },

  onItemMoved: function(aItemId,
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
  onBeforeItemRemoved: function PSB_onBeforeItemRemoved(aItemId) {},
  onItemVisited: function() {},

  onItemChanged: function PSB_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aNewValue) {
    if (aProperty !== "title")
      return;

    var views = getViewsForFolder(PlacesUtils.bookmarks.getFolderIdForItem(aItemId));
    ok(views.length > 0, "Found affected views (" + views.length + "): " + views);

    // Check that item has been moved in the correct position.
    let validator = function(aElementOrTreeIndex) {
      if (typeof(aElementOrTreeIndex) == "number") {
        var sidebar = document.getElementById("sidebar");
        var tree = sidebar.contentDocument.getElementById("bookmarks-view");
        let cellText = tree.view.getCellText(aElementOrTreeIndex,
                                             tree.columns.getColumnAt(0));
        if (!aNewValue)
          return cellText == PlacesUIUtils.getBestTitle(tree.view.nodeForTreeIndex(aElementOrTreeIndex));
        return cellText == aNewValue;
      }
      else {
        if (!aNewValue && aElementOrTreeIndex.localName != "toolbarbutton")
          return aElementOrTreeIndex.getAttribute("label") == PlacesUIUtils.getBestTitle(aElementOrTreeIndex._placesNode);
        return aElementOrTreeIndex.getAttribute("label") == aNewValue;
      }
    };

    for (var i = 0; i < views.length; i++) {
      var [node, index, valid] = searchItemInView(aItemId, views[i], validator);
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
  var toolbar = document.getElementById("PlacesToolbarItems");

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
        popup.showPopup(popup);
        var foundNode = findNode(popup);
        popup.hidePopup();
        if (foundNode[0] != null)
          return foundNode;
      }
    }
    return [null, null];
  }

  return findNode(toolbar);
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
    return [null, null, false]
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
      return ["toolbar", "sidebar"]
      break;
    case PlacesUtils.bookmarksMenuFolderId:
      return ["menu", "sidebar"]
      break;
    case PlacesUtils.unfiledBookmarksFolderId:
      return ["sidebar"]
      break;    
  }
  return new Array();
}
