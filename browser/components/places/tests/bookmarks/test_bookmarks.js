/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
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

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  onItemAdded: function(id, uri, folder, index) {
    this._itemAdded = uri;
    this._itemAddedId = id;
    this._itemAddedFolder = folder;
    this._itemAddedIndex = index;
  },
  onItemRemoved: function(id, uri, folder, index) {
    this._itemRemoved = uri;
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  onItemChanged: function(id, uri, property, value) {
    this._itemChangedId = id;
    this._itemChanged = uri;
    this._itemChangedProperty = property;
    this._itemChangedValue = value;
  },
  onItemVisited: function(uri, visitID, time) {
    this._itemVisited = uri;
    this._itemVisitedID = visitID;
    this._itemVisitedTime = time;
  },
  onFolderAdded: function(folder, parent, index) {
    this._folderAdded = folder;
    this._folderAddedParent = parent;
    this._folderAddedIndex = index;
  },
  onFolderRemoved: function(folder, parent, index) {
    this._folderRemoved = folder;
    this._folderRemovedParent = parent;
    this._folderRemovedIndex = index;
  },
  onFolderMoved: function(folder, oldParent, oldIndex, newParent, newIndex) {
    this._folderMoved = folder;
    this._folderMovedOldParent = oldParent;
    this._folderMovedOldIndex = oldIndex;
    this._folderMovedNewParent = newParent;
    this._folderMovedNewIndex = newIndex;
  },
  onFolderChanged: function(folder, property) {
    this._folderChanged = folder;
    this._folderChangedProperty = property;
  },
  onSeparatorAdded: function(folder, index) {
    this._separatorAdded = folder;
    this._separatorAddedIndex = index;
  },
  onSeparatorRemoved: function(folder, index) {
    this._separatorRemoved = folder;
    this._separatorRemovedIndex = index;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
};
bmsvc.addObserver(observer, false);

// get bookmarks root index
var root = bmsvc.bookmarksRoot;

// index at which items should begin
var bmStartIndex = 3;

// main
function run_test() {
  // test roots
  do_check_true(bmsvc.placesRoot > 0);
  do_check_true(bmsvc.bookmarksRoot > 0);
  do_check_true(bmsvc.toolbarRoot > 0);

  // insert item
  var newId = bmsvc.insertItem(root, uri("http://google.com/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId);
  do_check_eq(observer._itemAdded.spec, "http://google.com/");
  do_check_eq(observer._itemAddedFolder, root);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);

  // set item title
  bmsvc.setItemTitle(newId, "Google");
  do_check_eq(observer._itemChangedId, newId);
  do_check_eq(observer._itemChanged.spec, "http://google.com/");
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Google");

  // get item title
  var title = bmsvc.getItemTitle(newId);
  do_check_eq(title, "Google");

  // get item title bad input
  try {
    var title = bmsvc.getItemTitle(-3);
    do_throw("getItemTitle accepted bad input");
  } catch(ex) {}

  // test folder that the bm is in
  var folderId = bmsvc.getFolderIdForItem(newId);
  do_check_eq(folderId, root);

  // create folder
  var workFolder = bmsvc.createFolder(root, "Work", 3);
  do_check_eq(observer._folderAdded, workFolder);
  do_check_eq(observer._folderAddedParent, root);
  do_check_eq(observer._folderAddedIndex, 3);

  //XXX - test setFolderTitle
  //XXX - test getFolderTitle

  // insert item
  var newId2 = bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), 0);
  do_check_eq(observer._itemAddedId, newId2);
  do_check_eq(observer._itemAdded.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(newId2, "DevMo");
  do_check_eq(observer._itemChanged.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemChangedProperty, "title");

  // insert item
  var newId3 = bmsvc.insertItem(workFolder, uri("http://msdn.microsoft.com/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId3);
  do_check_eq(observer._itemAdded.spec, "http://msdn.microsoft.com/");
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);

  // change item
  bmsvc.setItemTitle(newId3, "MSDN");
  do_check_eq(observer._itemChanged.spec, "http://msdn.microsoft.com/");
  do_check_eq(observer._itemChangedProperty, "title");

  // remove item
  bmsvc.removeItem(newId2);
  do_check_eq(observer._itemRemovedId, newId2);
  do_check_eq(observer._itemRemoved.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemRemovedFolder, workFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  // insert item
  var newId4 = bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId4);
  do_check_eq(observer._itemAdded.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);
  
  // create folder
  var homeFolder = bmsvc.createFolder(root, "Home", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._folderAdded, homeFolder);
  do_check_eq(observer._folderAddedParent, root);
  do_check_eq(observer._folderAddedIndex, 5);

  // insert item
  var newId5 = bmsvc.insertItem(homeFolder, uri("http://espn.com/"), 0);
  do_check_eq(observer._itemAddedId, newId5);
  do_check_eq(observer._itemAdded.spec, "http://espn.com/");
  do_check_eq(observer._itemAddedFolder, homeFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(newId5, "ESPN");
  do_check_eq(observer._itemChanged.spec, "http://espn.com/");
  do_check_eq(observer._itemChangedProperty, "title");

  // insert item
  var newId6 = bmsvc.insertItem(root, uri("place:domain=google.com&group=1"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAdded.spec, "place:domain=google.com&group=1");
  do_check_eq(observer._itemAddedFolder, root);
  do_check_eq(observer._itemAddedIndex, 6);

  // change item
  bmsvc.setItemTitle(newId6, "Google Sites");
  do_check_eq(observer._itemChanged.spec, "place:domain=google.com&group=1");
  do_check_eq(observer._itemChangedProperty, "title");

  // move folder
  bmsvc.moveFolder(workFolder, root, bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._folderMoved, workFolder);
  do_check_eq(observer._folderMovedOldParent, root);
  do_check_eq(observer._folderMovedOldIndex, 3);
  do_check_eq(observer._folderMovedNewParent, root);
  do_check_eq(observer._folderMovedNewIndex, 6);

  // Test expected failure of moving a folder to be its own parent
  try {
    bmsvc.moveFolder(workFolder, workFolder, bmsvc.DEFAULT_INDEX);
    do_throw("moveFolder() allowed moving a folder to be it's own parent.");
  } catch (e) {}
  do_check_eq(bmsvc.indexOfFolder(root, workFolder), 6);

  // test insertSeparator
  // XXX - this should also query bookmarks for the folder children
  // and then test the node type at our index
  try {
    bmsvc.insertSeparator(root, 1);
    bmsvc.removeChildAt(root, 1);
  } catch(ex) {
    do_throw("insertSeparator: " + ex);
  }

  // test indexOfFolder
  var tmpFolder = bmsvc.createFolder(root, "tmp", 2);
  do_check_eq(bmsvc.indexOfFolder(root, tmpFolder), 2);

  // test setKeywordForURI
  var kwTestItemId = bmsvc.insertItem(root, uri("http://keywordtest.com"), bmsvc.DEFAULT_INDEX);
  try {
    var res = bmsvc.setKeywordForBookmark(kwTestItemId, "bar");
  } catch(ex) {
    do_throw("setKeywordForBookmark: " + ex);
  }

  // test getKeywordForBookmark
  var k = bmsvc.getKeywordForBookmark(kwTestItemId);
  do_check_eq("bar", k);

  // test getKeywordForURI
  var k = bmsvc.getKeywordForURI(uri("http://keywordtest.com/"));
  do_check_eq("bar", k);

  // test getURIForKeyword
  var u = bmsvc.getURIForKeyword("bar");
  do_check_eq("http://keywordtest.com/", u.spec);

  // test getBookmarkIdsForURI
  var newId8 = bmsvc.insertItem(root, uri("http://foo8.com/"), 2);
  var b = bmsvc.getBookmarkIdsForURI(uri("http://foo8.com/"), {});
  do_check_eq(b[0], newId8);

  // test removeFolderChildren
  // 1) add/remove each child type (bookmark, separator, folder)
  var tmpFolder = bmsvc.createFolder(root, "removeFolderChildren", bmsvc.DEFAULT_INDEX);
  bmsvc.insertItem(tmpFolder, uri("http://foo9.com/"), bmsvc.DEFAULT_INDEX);
  bmsvc.createFolder(tmpFolder, "subfolder", bmsvc.DEFAULT_INDEX);
  bmsvc.insertSeparator(tmpFolder, bmsvc.DEFAULT_INDEX);
  // 2) confirm that folder has 3 children
  try {
    var options = histsvc.getNewQueryOptions();
    options.maxResults = 1;
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var query = histsvc.getNewQuery();
    query.setFolders([tmpFolder], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    do_check_eq(rootNode.childCount, 3);
    rootNode.containerOpen = false;
  } catch(ex) { do_throw("removeFolderChildren(): " + ex); }
  // 3) remove all children
  bmsvc.removeFolderChildren(tmpFolder);
  // 4) confirm that folder has 0 children
  try {
    result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    do_check_eq(rootNode.childCount, 0);
    rootNode.containerOpen = false;
  } catch(ex) { do_throw("removeFolderChildren(): " + ex); }

  // test getItemURI
  var newId9 = bmsvc.insertItem(root, uri("http://foo9.com/"), bmsvc.DEFAULT_INDEX);
  var placeURI = bmsvc.getItemURI(newId9);
  do_check_eq(placeURI.spec, "place:moz_bookmarks.id=" + newId9 + "&group=3");

  // XXX - test folderReadOnly

  // test bookmark id in query output
  try {
    var options = histsvc.getNewQueryOptions();
    options.maxResults = 1;
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var query = histsvc.getNewQuery();
    query.setFolders([root], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    for (var i=0; i < cc; ++i) {
      var node = rootNode.getChild(i);
      do_check_true(node.bookmarkId > 0);
    }
    root.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }

  /* test that multiple bookmarks with same URI show up in queries
  try {
    // test uri
    var mURI = uri("http://multiple.uris.in.query");

    // add 2 bookmarks
    bmsvc.insertItem(root, mURI, bmsvc.DEFAULT_INDEX);
    bmsvc.insertItem(root, mURI, bmsvc.DEFAULT_INDEX);

    // query
    var options = histsvc.getNewQueryOptions();
    options.maxResults = 2;
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var query = histsvc.getNewQuery();
    query.setFolders([root], 1);
    query.uri = mURI;
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 2);
    root.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }
  */

  // test change bookmark uri
  var newId10 = bmsvc.insertItem(root, uri("http://foo10.com/"), bmsvc.DEFAULT_INDEX);
  bmsvc.changeBookmarkURI(newId10, uri("http://foo11.com/"));
  do_check_eq(observer._itemChangedId, newId10);
  do_check_eq(observer._itemChanged.spec, "http://foo11.com/");
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "");

  var newId11 = bmsvc.insertItem(root, uri("http://foo11.com/"), bmsvc.DEFAULT_INDEX);
  var bmURI = bmsvc.getBookmarkURI(newId11);
  do_check_eq("http://foo11.com/", bmURI.spec);

  // test getItemIndex
  var newId12 = bmsvc.insertItem(root, uri("http://foo11.com/"), 1);
  var bmIndex = bmsvc.getItemIndex(newId12);
  do_check_eq(1, bmIndex);
}

// XXXDietrich - get this section up to date

///  EXPECTED TABLE RESULTS
///  moz_bookmarks:
///  item_child    folder_child    parent    position
///  ----------    ------------    ------    --------
///                1               0         0
///                2               1         4
///                3               1         3
///  1                             1         0
///  2                             1         1
///  3                             1         2
///                4               2         0
///  4                             4         0
///  5                             4         1
///  6                             4         2
///  7                             4         3
///  8                             4         4
///  9                             4         5
///  10                            4         6
///  11                            4         7
///  12                            2         1
///                5               2         4
///  14                            5         0
///  15                            5         1
///                6               2         2
///  16                            6         0
///  17                            2         3
///
///  moz_history:
///  id            url
///  --            ------------------------
///  1             place:  // Viewed today
///  2             place:  // Viewed past week
///  3             place:  // All pages
///  4             http://start.mozilla.org/firefox
///  5             http://www.mozilla.org/products/firefox/central.html
///  6             http://addons.mozilla.org/?application=firefox
///  7             http://getfirefox.com/
///  8             http://www.mozilla.org/
///  9             http://www.mozillazine.org/
///  10            http://store.mozilla.org/
///  11            http://www.spreadfirefox.com/
///  12            http://google.com/
///  13            http://developer.mozilla.org/
///  14            http://msdn.microsoft.com/
///  15            http://developer.mozilla.org/devnews/
///  16            http://espn.com/
///  17            place:  // Google Sites
///  18            place:folder=5&group=3
///  19            place:folder=6&group=3
///
///  moz_bookmarks_folders:
///  id            name
///  --            -----------------------
///  1
///  2             Bookmarks Menu
///  3             Bookmarks Toolbar
///  4             Firefox and Mozilla Links
///  5             Work
///  6             Home
