/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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
 * Unit test for the bookmarks service.  Invoke the test like this:
 *  xpcshell -f testbookmarks.js
 */

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

// If there's no location registered for the profile direcotry, register one now.
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var profileDir = null;
try {
  profileDir = dirSvc.get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);
} catch (e) {}
if (!profileDir) {
  // Register our own provider for the profile directory.
  // It will simply return the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == NS_APP_USER_PROFILE_50_DIR) {
        return dirSvc.get("CurProcD", Ci.nsIFile);
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}

var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

function uri(spec) {
  return iosvc.newURI(spec, null, null);
}

dump("starting tests\n");

var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);

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
  onItemMoved: function(uri, folder, oldIndex, newIndex) {
    this._itemMoved = uri;
    this._itemMovedFolder = folder;
    this._itemMovedOldIndex = oldIndex;
    this._itemMovedNewIndex = newIndex;
  },
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

var root = bmsvc.bookmarksRoot;

// add some bookmarks and folders. note that the bookmarks menu starts
// with two items from the default bookmarks
// ("Quick Searches" and "Firefox and Mozilla links").

bmsvc.insertItem(root, uri("http://google.com/"), -1);
if (observer._itemAdded.spec != "http://google.com/" ||
    observer._itemAddedFolder != root || observer._itemAddedIndex != 2) {
  dump("insertItem notification 1 FAILED\n");
}
bmsvc.setItemTitle(uri("http://google.com/"), "Google");
if (observer._itemChanged.spec != "http://google.com/" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification 1 FAILED\n");
}

var workFolder = bmsvc.createFolder(root, "Work", 3);
if (observer._folderAdded != workFolder ||
    observer._folderAddedParent != root ||
    observer._folderAddedIndex != 3) {
  dump("createFolder notification 1 FAILED\n");
}
bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), 0);
if (observer._itemAdded.spec != "http://developer.mozilla.org/" ||
    observer._itemAddedFolder != workFolder || observer._itemAddedIndex != 0) {
  dump("insertItem notification 2 FAILED\n");
}
bmsvc.setItemTitle(uri("http://developer.mozilla.org/"), "DevMo");
if (observer._itemChanged.spec != "http://developer.mozilla.org/" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification 2 FAILED\n");
}
bmsvc.insertItem(workFolder, uri("http://msdn.microsoft.com/"), -1);
if (observer._itemAdded.spec != "http://msdn.microsoft.com/" ||
    observer._itemAddedFolder != workFolder || observer._itemAddedIndex != 1) {
  dump("insertItem notification 3 FAILED\n");
}
bmsvc.setItemTitle(uri("http://msdn.microsoft.com/"), "MSDN");
if (observer._itemChanged.spec != "http://msdn.microsoft.com/" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification 2 FAILED\n");
}
bmsvc.removeItem(workFolder, uri("http://developer.mozilla.org/"));
if (observer._itemRemoved.spec != "http://developer.mozilla.org/" ||
    observer._itemRemovedFolder != workFolder ||
    observer._itemRemovedIndex != 0) {
  dump("removeItem notification 1 FAILED\n");
}
if (observer._beginUpdateBatch != true) {
  dump("beginUpdateBatch notification 1 FAILED\n");
}
observer._beginUpdateBatch = false;
if (observer._itemMoved.spec != "http://msdn.microsoft.com/" ||
    observer._itemMovedFolder != workFolder ||
    observer._itemMovedOldIndex != 1 ||
    observer._itemMovedNewIndex != 0) {
  dump("itemMoved notification 1 FAILED\n");
}
if (observer._endUpdateBatch != true) {
  dump("endUpdateBatch notification 1 FAILED\n");
}
observer._endUpdateBatch = false;
bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), -1);
if (observer._itemAdded.spec != "http://developer.mozilla.org/" ||
    observer._itemAddedFolder != workFolder || observer._itemAddedIndex != 1) {
  dump("insertItem notification 4 FAILED\n");
}
bmsvc.replaceItem(workFolder, uri("http://developer.mozilla.org/"),
                  uri("http://developer.mozilla.org/devnews/"));
if (observer._itemReplaced.spec != "http://developer.mozilla.org/" ||
    observer._itemReplacedNew.spec != "http://developer.mozilla.org/devnews/" ||
    observer._itemReplacedFolder != workFolder) {
  dump("replaceItem notification 1 FAILED\n");
}
var homeFolder = bmsvc.createFolder(root, "Home", -1);
if (observer._folderAdded != homeFolder ||
    observer._folderAddedParent != root || observer._folderAddedIndex != 4) {
  dump("createFolder notification 2 FAILED\n");
}
bmsvc.insertItem(homeFolder, uri("http://espn.com/"), 0);
if (observer._itemAdded.spec != "http://espn.com/" ||
    observer._itemAddedFolder != homeFolder || observer._itemAddedIndex != 0) {
  dump("insertItem notification 5 FAILED\n");
}
bmsvc.setItemTitle(uri("http://espn.com/"), "ESPN");
if (observer._itemChanged.spec != "http://espn.com/" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification 3 FAILED\n");
}
bmsvc.insertItem(root, uri("place:domain=google.com&group=1"), -1);
if (observer._itemAdded.spec != "place:domain=google.com&group=1" ||
    observer._itemAddedFolder != root || observer._itemAddedIndex != 5) {
  dump("insertItem notification 6 FAILED\n");
}
bmsvc.setItemTitle(uri("place:domain=google.com&group=1"), "Google Sites");
if (observer._itemChanged.spec != "place:domain=google.com&group=1" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification 4 FAILED\n");
}
bmsvc.moveFolder(workFolder, root, -1);
if (observer._folderMoved != workFolder ||
    observer._folderMovedOldParent != root ||
    observer._folderMovedOldIndex != 3 ||
    observer._folderMovedNewParent != root ||
    observer._folderMovedNewIndex != 5) {
  dump("moveFolder notification 1 FAILED\n");
}
// Test expected failure of moving a folder to be its own parent
try {
  bmsvc.moveFolder(workFolder, workFolder, -1);
  dump("moveFolder parameter validation 1 FAILED\n");
} catch (e) {}
if (bmsvc.indexOfItem(root, uri("http://google.com/")) != 2) {
  dump("indexOfItem 1 FAILED\n");
}
if (bmsvc.indexOfFolder(root, workFolder) != 5) {
  dump("indexOfFolder 1 FAILED\n");
}
// XXX test folderReadOnly

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
