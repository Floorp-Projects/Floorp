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

const NS_STORAGE_FILE = "UStor";
const nsIFile = Components.interfaces.nsIFile;

// If there's no location registered for the storage file, register one now.
var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);
var storageFile = null;
try {
  storageFile = dirSvc.get(NS_STORAGE_FILE, nsIFile);
} catch (e) {}
if (!storageFile) {
  // Register our own provider for the storage file.  It will create the file
  // "storage.sdb" in the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == NS_STORAGE_FILE) {
        var file = dirSvc.get("CurProcD", nsIFile);
        file.append("storage.sdb");
        return file;
      }
      throw Components.results.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsIDirectoryServiceProvider) ||
          iid.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Components.interfaces.nsIDirectoryService).registerProvider(provider);
}

var iosvc = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);

function uri(spec) {
  return iosvc.newURI(spec, null, null);
}

dump("starting tests\n");

var bmsvc = Components.classes["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Components.interfaces.nsINavBookmarksService);

var observer = {
  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  get wantAllDetails() { return this._wantAllDetails; },
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
  onItemChanged: function(uri, property) {
    this._itemChanged = uri;
    this._itemChangedProperty = property;
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
  onFolderMoved: function(folder, parent, index) {
    this._folderMoved = folder;
    this._folderMovedParent = parent;
    this._folderMovedIndex = index;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsINavBookmarkObserver) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  _wantAllDetails: true
};

bmsvc.addObserver(observer);

var root = bmsvc.bookmarksRoot;

// add some bookmarks and folders

bmsvc.insertItem(root, uri("http://www.mozilla.org/"), 0);
if (observer._itemAdded.spec != "http://www.mozilla.org/" ||
    observer._itemAddedFolder != root || observer._itemAddedIndex != 0) {
  dump("insertItem notification FAILED\n");
}
bmsvc.setItemTitle(uri("http://www.mozilla.org/"), "Mozilla.org");
if (observer._itemChanged.spec != "http://www.mozilla.org/" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification FAILED\n");
}
bmsvc.insertItem(root, uri("http://google.com/"), -1);
if (observer._itemAdded.spec != "http://google.com/" ||
    observer._itemAddedFolder != root || observer._itemAddedIndex != 1) {
  dump("insertItem notification FAILED\n");
}
bmsvc.setItemTitle(uri("http://google.com/"), "Google");
if (observer._itemChanged.spec != "http://google.com/" ||
    observer._itemChangedProperty != "title") {
  dump("setItemTitle notification FAILED\n");
}
bmsvc.removeItem(root, uri("http://www.mozilla.org/"));
if (observer._itemRemoved.spec != "http://www.mozilla.org/" ||
    observer._itemRemovedFolder != root || observer._itemRemovedIndex != 0) {
  dump("removeItem notification FAILED\n");
}
bmsvc.insertItem(root, uri("http://www.mozilla.org/"), -1);
if (observer._itemAdded.spec != "http://www.mozilla.org/" ||
    observer._itemAddedFolder != root || observer._itemAddedIndex != 1) {
  dump("insertItem notification FAILED\n");
}

///  EXPECTED TABLE RESULTS
///  moz_bookmarks_assoc:
///  item_child    folder_child    parent    position
///  ----------    ------------    ------    --------
///                1
///  2                             1         0
///  1                             1         1
///
///  moz_history:
///  id            url
///  --            ------------------------
///  1             http://www.mozilla.org/
///  2             http://google.com/
///
///  moz_bookmarks_containers:
///  id
///  --
//   1
