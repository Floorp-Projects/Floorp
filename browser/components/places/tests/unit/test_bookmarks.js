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

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  onItemAdded: function(uri, folder, index) {
    this._itemAdded = uri;
    this._itemAddedFolder = folder;
    this._itemAddedIndex = index;
  },
  onItemRemoved: function(uri, folder, index) {
    this._itemRemoved = uri;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  /* XXXDietrich - vestigial? is not currently in nsINavBookmarksService
  onItemMoved: function(uri, folder, oldIndex, newIndex) {
    this._itemMoved = uri;
    this._itemMovedFolder = folder;
    this._itemMovedOldIndex = oldIndex;
    this._itemMovedNewIndex = newIndex;
  },
  */
  onItemChanged: function(uri, property, value) {
    this._itemChanged = uri;
    this._itemChangedProperty = property;
    this._itemChangedValue = value;
  },
  onItemVisited: function(uri, visitID, time) {
    this._itemVisited = uri;
    this._itemVisitedID = visitID;
    this._itemVisitedTime = time;
  },
  onItemReplaced: function(folder, oldItem, newItem) {
    this._itemReplacedFolder = folder;
    this._itemReplaced = oldItem;
    this._itemReplacedNew = newItem;
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
var bmStartIndex = 3

// main
function run_test() {
  // insert item
  bmsvc.insertItem(root, uri("http://google.com/"), -1);
  do_check_eq(observer._itemAdded.spec, "http://google.com/");
  do_check_eq(observer._itemAddedFolder, root);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);

  // change item
  bmsvc.setItemTitle(uri("http://google.com/"), "Google");
  do_check_eq(observer._itemChanged.spec, "http://google.com/");
  do_check_eq(observer._itemChangedProperty, "title");

  // create folder
  var workFolder = bmsvc.createFolder(root, "Work", 3);
  do_check_eq(observer._folderAdded, workFolder);
  do_check_eq(observer._folderAddedParent, root);
  do_check_eq(observer._folderAddedIndex, 3);

  // insert item
  bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), 0);
  do_check_eq(observer._itemAdded.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(uri("http://developer.mozilla.org/"), "DevMo");
  do_check_eq(observer._itemChanged.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemChangedProperty, "title");

  // insert item
  bmsvc.insertItem(workFolder, uri("http://msdn.microsoft.com/"), -1);
  do_check_eq(observer._itemAdded.spec, "http://msdn.microsoft.com/");
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);

  // change item
  bmsvc.setItemTitle(uri("http://msdn.microsoft.com/"), "MSDN");
  do_check_eq(observer._itemChanged.spec, "http://msdn.microsoft.com/");
  do_check_eq(observer._itemChangedProperty, "title");

  // remove item
  bmsvc.removeItem(workFolder, uri("http://developer.mozilla.org/"));
  do_check_eq(observer._itemRemoved.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemRemovedFolder, workFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  // insert item
  bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), -1);
  do_check_eq(observer._itemAdded.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);
  
  // replace item
  bmsvc.replaceItem(workFolder, uri("http://developer.mozilla.org/"),
    uri("http://developer.mozilla.org/devnews/"));
  do_check_eq(observer._itemReplaced.spec, "http://developer.mozilla.org/");
  do_check_eq(observer._itemReplacedNew.spec, "http://developer.mozilla.org/devnews/");
  do_check_eq(observer._itemReplacedFolder, workFolder);

  // create folder
  var homeFolder = bmsvc.createFolder(root, "Home", -1);
  do_check_eq(observer._folderAdded, homeFolder);
  do_check_eq(observer._folderAddedParent, root);
  do_check_eq(observer._folderAddedIndex, 5);

  // insert item
  bmsvc.insertItem(homeFolder, uri("http://espn.com/"), 0);
  do_check_eq(observer._itemAdded.spec, "http://espn.com/");
  do_check_eq(observer._itemAddedFolder, homeFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(uri("http://espn.com/"), "ESPN");
  do_check_eq(observer._itemChanged.spec, "http://espn.com/");
  do_check_eq(observer._itemChangedProperty, "title");

  // insert item
  bmsvc.insertItem(root, uri("place:domain=google.com&group=1"), -1);
  do_check_eq(observer._itemAdded.spec, "place:domain=google.com&group=1");
  do_check_eq(observer._itemAddedFolder, root);
  do_check_eq(observer._itemAddedIndex, 6);

  // change item
  bmsvc.setItemTitle(uri("place:domain=google.com&group=1"), "Google Sites");
  do_check_eq(observer._itemChanged.spec, "place:domain=google.com&group=1");
  do_check_eq(observer._itemChangedProperty, "title");

  // move folder
  bmsvc.moveFolder(workFolder, root, -1);
  do_check_eq(observer._folderMoved, workFolder);
  do_check_eq(observer._folderMovedOldParent, root);
  do_check_eq(observer._folderMovedOldIndex, 3);
  do_check_eq(observer._folderMovedNewParent, root);
  do_check_eq(observer._folderMovedNewIndex, 6);

  // Test expected failure of moving a folder to be its own parent
  try {
    bmsvc.moveFolder(workFolder, workFolder, -1);
    do_throw("moveFolder() allowed moving a folder to be it's own parent.");
  } catch (e) {}
  do_check_eq(bmsvc.indexOfItem(root, uri("http://google.com/")), 3);
  do_check_eq(bmsvc.indexOfFolder(root, workFolder), 6);
}

// XXX test folderReadOnly
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
