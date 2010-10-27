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
 * Tests Library Left pane view for liveupdate.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

var gLibrary = null;

function test() {
  waitForExplicitFinish();
  // This test takes quite some time, and timeouts frequently, so we require
  // more time to run.
  // See Bug 525610.
  requestLongerTimeout(2);

  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in context");
  ok(PlacesUIUtils, "PlacesUIUtils in context");

  // Open Library, we will check the left pane.
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened")
      return;
    ww.unregisterNotification(windowObserver);
    gLibrary = aSubject.QueryInterface(Ci.nsIDOMWindow);
    gLibrary.addEventListener("load", function onLoad(event) {
      gLibrary.removeEventListener("load", onLoad, false);
      executeSoon(startTest);
    }, false);
  }
  ww.registerNotification(windowObserver);
  ww.openWindow(null,
                "chrome://browser/content/places/places.xul",
                "",
                "chrome,toolbar=yes,dialog=no,resizable",
                null);
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
  ok(true, "*** Acting on menu bookmarks");
  var id = bs.insertBookmark(bs.bookmarksMenuFolder,
                             PlacesUtils._uri("http://bm1.mozilla.org/"),
                             bs.DEFAULT_INDEX,
                             "bm1");
  addedBookmarks.push(id);
  id = bs.insertBookmark(bs.bookmarksMenuFolder,
                         PlacesUtils._uri("place:"),
                         bs.DEFAULT_INDEX,
                         "bm2");
  bs.setItemTitle(id, "bm2_edited");
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
  addedBookmarks.push(id);
  bs.moveItem(id, bs.bookmarksMenuFolder, 0);

  // TOOLBAR
  ok(true, "*** Acting on toolbar bookmarks");
  bs.insertBookmark(bs.toolbarFolder,
                    PlacesUtils._uri("http://tb1.mozilla.org/"),
                    bs.DEFAULT_INDEX,
                    "tb1");
  bs.setItemTitle(id, "tb1_edited");
  addedBookmarks.push(id);
  id = bs.insertBookmark(bs.toolbarFolder,
                         PlacesUtils._uri("place:"),
                         bs.DEFAULT_INDEX,
                         "tb2");
  bs.setItemTitle(id, "tb2_edited");
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
                         "bmf1");
  addedBookmarks.push(id);
  bs.moveItem(id, bs.toolbarFolder, 0);

  // UNSORTED
  ok(true, "*** Acting on unsorted bookmarks");
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
                         "ubf1");
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
  // Close Library window.
  gLibrary.close();
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
  onItemAdded: function PSB_onItemAdded(aItemId, aFolderId, aIndex, aURI) {
    var node = null;
    var index = null;
    [node, index] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places);
    // Left pane should not be updated for normal bookmarks or separators.
    var type = PlacesUtils.bookmarks.getItemType(aItemId);
    switch (type) {
      case PlacesUtils.bookmarks.TYPE_BOOKMARK:
        var uriString = PlacesUtils.bookmarks.getBookmarkURI(aItemId).spec;
        var isQuery = uriString.substr(0, 6) == "place:";
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
    var node = null;
    var index = null;
    [node, index] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places);
    is(node, null, "Places node not found in left pane");
  },

  onItemMoved: function(aItemId,
                        aOldFolderId, aOldIndex,
                        aNewFolderId, aNewIndex) {
    var node = null;
    var index = null;
    [node, index] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places);
    // Left pane should not be updated for normal bookmarks or separators.
    var type = PlacesUtils.bookmarks.getItemType(aItemId);
    switch (type) {
      case PlacesUtils.bookmarks.TYPE_BOOKMARK:
        var uriString = PlacesUtils.bookmarks.getBookmarkURI(aItemId).spec;
        var isQuery = uriString.substr(0, 6) == "place:";
        if (isQuery) {
          isnot(node, null, "Found new Places node in left pane");
          ok(index >= 0, "Node is at index " + index);
          break;
        }
        // Fallback to separator case if this is not a query.
      case type == PlacesUtils.bookmarks.TYPE_SEPARATOR:
        is(node, null, "New Places node not added in left pane");
        break;
      default:
        isnot(node, null, "Found new Places node in left pane");
        ok(index >= 0, "Node is at index " + index);
    }
  },

  onBeginUpdateBatch: function PSB_onBeginUpdateBatch() {},
  onEndUpdateBatch: function PSB_onEndUpdateBatch() {},
  onBeforeItemRemoved: function PSB_onBeforeItemRemoved(aItemId) {},
  onItemVisited: function() {},
  onItemChanged: function PSB_onItemChanged(aItemId, aProperty,
                                            aIsAnnotationProperty, aNewValue) {
    if (aProperty == "title") {
      let validator = function(aTreeRowIndex) {
        let tree = gLibrary.PlacesOrganizer._places;
        let cellText = tree.view.getCellText(aTreeRowIndex,
                                             tree.columns.getColumnAt(0));
        return cellText == aNewValue;
      }
      let [node, index, valid] = getNodeForTreeItem(aItemId, gLibrary.PlacesOrganizer._places, validator);
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
    for (var i = aContainerIndex + 1; i < aTree.view.rowCount; i++) {
      var node = aTree.view.nodeForTreeIndex(i);

      if (node.itemId == aItemId) {
        // Minus one because we want relative index inside the container.
        let valid = aValidator ? aValidator(i) : true;
        return [node, i - aTree.view.getParentIndex(i) - 1, valid];
      }

      if (PlacesUtils.nodeIsFolder(node)) {
        // Open container.
        aTree.view.toggleOpenState(i);
        // Search inside it.
        var foundNode = findNode(i);
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
    return [null, null, false]
  }

  // Root node is hidden, so we need to manually walk the first level.
  for (var i = 0; i < aTree.view.rowCount; i++) {
    // Open container.
    aTree.view.toggleOpenState(i);
    // Search inside it.
    var foundNode = findNode(i);
    // Close container.
    aTree.view.toggleOpenState(i);
    // Return node if found.
    if (foundNode[0] != null)
      return foundNode;
  }
  return [null, null, false];
}
