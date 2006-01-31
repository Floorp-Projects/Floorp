//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

var PlacesBrowserShim = {
  _bms: null,
  _lms: null,
  _history: null,
};

PlacesBrowserShim.init = function PBS_init() {
  var addBookmarkCmd = document.getElementById("Browser:AddBookmarkAs");
  addBookmarkCmd.setAttribute("oncommand", "PlacesBrowserShim.addBookmark()");

  this._bms = 
      Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService);

  this._lms = 
      Cc["@mozilla.org/browser/livemark-service;1"].
      getService(Ci.nsILivemarkService);
      
  this._hist = 
      Cc["@mozilla.org/browser/nav-history-service;1"].
      getService(Ci.nsINavHistoryService);

  // Override the old addLivemark function
  BookmarksUtils.addLivemark = function(a,b,c,d) {PlacesBrowserShim.addLivemark(a,b,c,d);};
  
  var newMenuPopup = document.getElementById("bookmarksMenuPopup");
  var query = this._hist.getNewQuery();
  query.setFolders([this._bms.bookmarksRoot], 1);
  var options = this._hist.getNewQueryOptions();
  options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
  options.expandQueries = true;
  var result = this._hist.executeQuery(query, options);
  newMenuPopup._result = result;
  newMenuPopup._resultNode = result.root;

  window.controllers.appendController(PlacesController);
  
  PlacesController.topWindow = window;
  PlacesController.tm = PlacesTransactionManager;
};

PlacesBrowserShim.addBookmark = function PBS_addBookmark() {
  var selectedBrowser = getBrowser().selectedBrowser;
  this._bookmarkURI(this._bms.bookmarksRoot, selectedBrowser.currentURI, 
                    selectedBrowser.contentTitle);
};

PlacesBrowserShim._bookmarkURI = 
function PBS__bookmarkURI(folder, uri, title) {
  this._bms.insertItem(folder, uri, -1);
  this._bms.setItemTitle(uri, title);
};

PlacesBrowserShim.showHistory = function PBS_showHistory() {
  loadURI("chrome://browser/content/places/places.xul?history", null, null);
};

PlacesBrowserShim.showBookmarks = function PBS_showBookmarks() {
  loadURI("chrome://browser/content/places/places.xul?bookmarks", null, null);
};

PlacesBrowserShim.addLivemark = function PBS_addLivemark(aURL, aFeedURL, aTitle, aDescription) {
  // TODO -- put in nice confirmation dialog.
  var ios = 
      Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);

  this._lms.createLivemark(this._bms.toolbarRoot,
                          aTitle,
                          ios.newURI(aURL, null, null),
                          ios.newURI(aFeedURL, null, null),
                          -1);
};

/**
 * This is a custom implementation of nsITransactionManager. We do not chain 
 * or aggregate the default implementation because the order in which 
 * transactions are performed and undone is important to the user experience. 
 * There are two classes of transactions - those done by the browser window 
 * that contains this transaction manager, and those done by the embedded 
 * Places page. All transactions done in either part of the UI are recorded 
 * here, but ones performed by actions taken in the Places page affect the
 * Undo/Redo menu items and keybindings in the browser window only when the
 * Places page is the active tab. This is to prevent the user from accidentally
 * undoing/redoing their changes while the Places page is not selected, and the
 * user not noticing. 
 * 
 * When the Places page is navigated away from, the undo items registered for
 * it are destroyed and the ability to undo those actions ceases. 
 */
var PlacesTransactionManager = {
  _undoItems: [],
  _redoItems: [],
  
  hidePageTransactions: true,
  
  _getNextVisibleIndex: function PTM__getNextVisibleItem(list) {
    if (!this.hidePageTransactions)
      return list.length - 1;
      
    for (var i = list.length - 1; i >= 0; --i) {
      if (!list[i].pageTransaction)
        return i;
    }
    return -1;
  },
  
  updateCommands: function PTM__updateCommands() {
    CommandUpdater.updateCommand("cmd_undo");
    CommandUpdater.updateCommand("cmd_redo");
  },

  doTransaction: function PTM_doTransaction(transaction) {
    transaction.doTransaction();
    this._undoItems.push(transaction);
    this._redoItems = [];
    this.updateCommands();
  },
  
  undoTransaction: function PTM_undoTransaction() {
    var index = this._getNextVisibleIndex(this._undoItems);
    ASSERT(index >= 0, "Invalid Transaction index");
    var transaction = this._undoItems.splice(index, 1)[0];
    transaction.undoTransaction();
    this._redoItems.push(transaction);
    this.updateCommands();
  },

  redoTransaction: function PTM_redoTransaction() {
    var index = this._getNextVisibleIndex(this._redoItems);
    ASSERT(index >= 0, "Invalid Transaction index");
    var transaction = this._redoItems.splice(index, 1)[0];
    transaction.redoTransaction();
    this._undoItems.push(transaction);    
    this.updateCommands();
  },
  
  clear: function PTM_clear() {
    this._undoItems = [];
    this._redoItems = [];
    this.updateCommands();
  },
  
  beginBatch: function PTM_beginBatch() {
  },
  
  endBatch: function PTM_endBatch() {
  },
  
  get numberOfUndoItems() {
    return this.getUndoList().numItems;
  },
  get numberOfRedoItems() {
    return this.getRedoList().numItems;
  },
  
  maxTransactionCount: -1,
  
  peekUndoStack: function PTM_peekUndoStack() {
    var index = this._getNextVisibleIndex(this._undoItems);
    ASSERT(index >= 0, "Invalid Transaction index");
    return this._undoItems[index];
  },
  peekRedoStack: function PTM_peekRedoStack() {
    var index = this._getNextVisibleIndex(this._redoItems);
    ASSERT(index >= 0, "Invalid Transaction index");
    return this._redoItems[index];
  },
  
  _filterList: function PTM__filterList(list) {
    if (!this.hidePageTransactions)
      return list;
    
    var transactions = [];
    for (var i = 0; i < list.length; ++i) {
      if (!list[i].pageTransaction)
        transactions.push(list[i]);
    }
    return transactions;
  },
  
  getUndoList: function PTM_getUndoList() {
    return new TransactionList(this._filterList(this._undoItems));
  },
  getRedoList: function PTM_getRedoList() {
    return new TransactionList(this._filterList(this._redoItems));
  },
  
  _listeners: [],  
  AddListener: function PTM_AddListener(listener) {
    this._listeners.push(listener);
  },
  RemoveListener: function PTM_RemoveListener(listener) {
    for (var i = 0; i < this._listeners.length; ++i) {
      if (this._listeners[i] == listener)
        this._listeners.splice(i, 1);
    }
  },

  QueryInterface: function PTM_QueryInterface(iid) {
    if (iid.equals(Ci.nsITransactionManager) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NOINTERFACE;
  }
};

function TransactionList(transactions) {
  this._transactions = transactions;
}
TransactionList.prototype = {
  get numItems() {
    return this._transactions.length;
  },
  
  itemIsBatch: function TL_itemIsBatch(index) {
    return false;
  },
  
  getItem: function TL_getItem(index) {
    return this._transactions[i];
  },
  
  getNumChildrenForItem: function TL_getNumChildrenForItem(index) {
    return 0;
  },
  
  getChildListForItem: function TL_getChildListForItem(index) {
    return null;
  },
  
  QueryInterface: function TL_QueryInterface(iid) {
    if (iid.equals(Ci.nsITransactionList) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NOINTERFACE;
  }
};

addEventListener("load", function () { PlacesBrowserShim.init(); }, false);
