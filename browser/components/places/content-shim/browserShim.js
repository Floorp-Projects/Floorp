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
  _bms: null,  // Bookmark Service
  _lms: null,  // Livemark Service
  _hist: null, // History Service
  _ios: null,  // IO Service, useful for making nsIURI objects
  _currentURI: null, // URI of the bookmark being modified
  _strings: null, // Localization string bundle
  MAX_INDENT_DEPTH: 6, // maximum indentation level of "tag" display

  init: function PBS_init() {
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

    this._ios =
    Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

    this._strings = document.getElementById("placeBundle");

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

    this._registerEventHandlers();

    window.controllers.appendController(PlacesController);

    PlacesController.topWindow = window;
    PlacesController.tm = PlacesTransactionManager;
  },


  addBookmark: function PBS_addBookmark() {
    var selectedBrowser = getBrowser().selectedBrowser;
    this._bookmarkURI(this._bms.bookmarksRoot, selectedBrowser.currentURI, 
                      selectedBrowser.contentTitle);
  },

  _bookmarkURI: function PBS__bookmarkURI(folder, uri, title) {
    this._bms.insertItem(folder, uri, -1);
    this._bms.setItemTitle(uri, title);
  },

  showHistory: function PBS_showHistory() {
    loadURI("chrome://browser/content/places/places.xul?history", null, null);
  },

  showBookmarks: function PBS_showBookmarks() {
    loadURI("chrome://browser/content/places/places.xul?bookmarks", null, null);
  },

  addLivemark: function PBS_addLivemark(url, feedURL, title, description) {
  // TODO -- put in nice confirmation dialog.

    this._lms.createLivemark(this._bms.toolbarRoot,
                             title,
                             this._makeURI(url),
                             this._makeURI(feedURL),
                             -1);
  },

  /**
   * Makes an nsIURI object from a string containing a URI.
   */

  _makeURI: function PBS__makeURI(urlString) {
    return this._ios.newURI(urlString, null, null);
  },


  /**
   * Gets the URI that the visible browser tab is rendering.
   *
   * @returns a string containing the URI currently being shown
   */

  _getCurrentLocation: function PBS__getCurrentLocation() {
    return getBrowser().selectedBrowser.webNavigation.currentURI;
  },

  /**
   * This method updates the state of navigation buttons (i.e. bookmark, feeds)
   * which depend on the location and contents of the current page.
   */

  _updateControlStates: function PBS__updateControlStates() {
    var feedButton = document.getElementById("places-subscribe");
    var bookmarkButton = document.getElementById("places-bookmark");

    if (this._bms.isBookmarked(this._getCurrentLocation())) {
      bookmarkButton.label = this._strings.getString("locationStatusBookmarked");
    } else {
      bookmarkButton.label = this._strings.getString("locationStatusNotBookmarked");
    }

    if (gBrowser.selectedBrowser.feeds) {
      feedButton.removeAttribute("disabled");
    } else {
      feedButton.setAttribute("disabled", "true");
    }
  },

  /**
   * Register the event handlers that Places needs to update its state.
   */

  _registerEventHandlers: function PBS_registerEventHandlers() {
    var self = this;

    function onPageShow(e){
      self.onPageShow(e);
    }
    getBrowser().addEventListener("pageshow", onPageShow, true);

    function onTabSwitch(e) {
      self.onTabSwitch(e);
    }
    getBrowser().mTabBox.addEventListener("select", onTabSwitch, true);
  },

  /**
   * This method should be called when the location currently being
   * rendered by a browser changes (loading new page or forward/back).
   */

  onPageShow: function PBS_onPageShow(e) {
    if (e.target && e.target.nodeName == "#document") {
      this._updateControlStates();
    }
  },

  /**
   * This method should be called when the user switches active tabs.
   */

  onTabSwitch: function PBS_onTabSwitch(e) {
    if (e.target == null || e.target.localName != "tabs")
    return;

    this._updateControlStates();
  },

  /**
   * This method should be called when the bookmark button is clicked.
   */

  onBookmarkButtonClick: function PBS_onBookmarkButtonClick() {
    var urlbar = document.getElementById("urlbar");
    this._currentURI = this._makeURI(urlbar.value);
    if (this._bms.isBookmarked(this._currentURI)) {
      this.showBookmarkProperties();
    } else {
      this._bms.insertItem(this._bms.bookmarksRoot, this._currentURI, -1);
      this._updateControlStates();
    }
  },

  showBookmarkProperties: function PBS_showBookmarkProperties() {
    var popup = document.getElementById("places-info-popup");
    var win = document.getElementById("main-window");
    var urlbar = document.getElementById("urlbar");

    var nurl = document.getElementById("edit-urlbar");
    var nurlbox = document.getElementById("edit-urlbox");

    var titlebox = document.getElementById("edit-titlebox");

    nurl.value = urlbar.value;
    nurl.label = urlbar.value;
    nurl.size = urlbar.size;
    nurlbox.style.left = urlbar.boxObject.x + "px";
    nurlbox.style.top = urlbar.boxObject.y + "px";
    nurl.height = urlbar.boxObject.height;
    nurl.style.width = urlbar.boxObject.width + "px";

    var title = this._bms.getItemTitle(this._makeURI(urlbar.value));
    titlebox.value = title;

    var query = this._hist.getNewQuery();
    query.setFolders([this._bms.placesRoot], 1);
    var options = this._hist.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeItems = true;

    this._result = this._hist.executeQuery(query, options);

    var tagArea = document.getElementById("tagbox");

    while (tagArea.hasChildNodes()) {
      tagArea.removeChild(tagArea.firstChild);
    }

    var elementDict = {};

    var root = this._result.root; // Note that root is always a container.
    root.containerOpen = true;
    this._populateTags(root, 0, tagArea, elementDict);
    root.containerOpen = false;

    var categories = this._bms.getBookmarkFolders(this._makeURI(urlbar.value), {});

    this._updateFolderTextbox(this._makeURI(urlbar.value));

    var length = 0;
    for (key in elementDict) {
      length++;
    }

    for (var i=0; i < categories.length; i++) {
      var elm = elementDict[categories[i]];
      elm.setAttribute("selected", "true");
    }

    popup.hidden = false;
    popup.style.left = "" + (nurlbox.boxObject.x - 5) + "px";
    popup.style.top = "" + (nurlbox.boxObject.y - 5) + "px";
    popup.style.width = "" + (nurlbox.boxObject.width + (5 * 2)) + "px";
    var pio = document.getElementById("places-info-options");
    pio.style.top = nurlbox.boxObject.y + nurlbox.boxObject.height + 5 + "px";
    pio.style.width = nurlbox.boxObject.width + "px";
    pio.style.left = nurlbox.boxObject.x + "px";
    var pig = document.getElementById("places-info-grid");
    pig.style.width = nurlbox.boxObject.width + "px";
    popup.style.height = (pio.boxObject.y + pio.boxObject.height - popup.boxObject.y + 5) + "px";
  },

  /**
   * This method is called to exit the Bookmark Properties panel.
   *
   * @param aSaveChanges boolean, should be true if changes performed while
   *                     the panel was active should be saved
   */

  hideBookmarkProperties:
  function PBS_hideBookmarkProperties(saveChanges) {
    if (saveChanges) {
      var titlebox = document.getElementById("edit-titlebox");
      this._bms.setItemTitle(this._currentURI, titlebox.value);
    }

    var popup = document.getElementById("places-info-popup");
    this._updateControlStates();
    popup.hidden = true;
  },


  /**
   * This method deletes the bookmark corresponding to the URI stored
   * in _currentURI.  _currentURI represents the URI that the Bookmark
   * Properties panel is currently viewing/editing.  Therefore, this method
   * is only relevant in when the Bookmark Properties panel is active.
   */

  deleteBookmark: function PBS_deleteBookmark() {
    if (!this._currentURI)
      return;

    var folders = this._bms.getBookmarkFolders(this._currentURI, {});
    if (folders.length == 0)
      return;

    this._bms.beginUpdateBatch();
    for (var i = 0; i < folders.length; i++) {
      this._bms.removeItem(folders[i], this._currentURI);
    }
    this._bms.endUpdateBatch();
  },

  /**
   * This method sets the contents of the "Folders" textbox in the
   * Bookmark Properties panel.
   *
   * @param aURI an nsIURI object representing the current bookmark's URI
   */

  _updateFolderTextbox: function PBS__updateFolderTextbox(uri) {
    var folderTextbox = document.getElementById("places-folder-list");
    folderTextbox.value = this._getFolderNameListForURI(uri);
  },

  /**
   * This method gets the list of folders that contain the current bookmark.
   *
   * @param aURI a nsIURI object representing the URI of the current bookmark
   *
   * @returns a comma-separated list of folder names in string form
   */

  _getFolderNameListForURI: function PBS__getFolderNameListForURI(uri) {
    var folders = this._bms.getBookmarkFolders(uri, {});
    var results = [];
    for (var i = 0; i < folders.length; i++) {
      results.push(this._bms.getFolderTitle(folders[i]));
    }
    return results.join(", ");
  },

  /**
   * Recursively populates the tag-like set of clickable folders.
   *
   * @param aContainer a reference to an nsINavHistoryContainerResultNode
   *        (whose) containerOpen property is set to true) representing
   *        the roote of the bookmark folder tree
   * @param aDepth the current iteration depth -- pass this 0 at the top level.
   *        This only affects the visual indentation level of the tag display.
   * @param aParentElement a vbox element into which the tags will be populated
   * @param aElementDict a dictionary mapping folder IDs to element references
   *        to be populated in this method
   *
   * @returns none
   */

  _populateTags:
  function PBS__populateTags (container, depth, parentElement, elementDict) {
    if (!container.containerOpen) {
      throw new Error("The containerOpen property of the container parameter should be set to true before calling populateTags(), and then set to false again afterwards.");
    }

    var row = null;
    for (var i = 0; i < container.childCount; i++) {
      var childNode = container.getChild(i);

      if (childNode.type != childNode.RESULT_TYPE_FOLDER)
        continue;

      var childFolder =
        childNode.QueryInterface(Ci.nsINavHistoryFolderResultNode);
      childFolder.containerOpen = true;

      // If we can't alter it, no use showing it as an option.
      if (childFolder.childrenReadOnly) {
        childFolder.containerOpen = false;
        continue;
      }

      if (childFolder.hasChildren) {
        row = document.createElement("hbox");
        row.setAttribute("class", "l" + depth);
        var tag = this._createTagElement(childFolder, false);
        elementDict[childFolder.folderId] = tag;
        tag.setAttribute("isparent", "true");
        row.appendChild(tag);
        parentElement.appendChild(row);
        row = null;
        var nextDepth = depth + 1;
        // We're limiting max indentation level here.
        if (nextDepth > this.MAX_INDENT_DEPTH)
          nextDepth = this.MAX_INDENT_DEPTH;
        this._populateTags(childFolder, nextDepth, parentElement, elementDict);
      } else {
        if (row == null) {
          row = document.createElement("description");
          row.setAttribute("class", "l" + depth);
          parentElement.appendChild(row);
        } else {
          // we now know that there must"ve been a tag before us on the same row
          var separator = document.createElement("label");
          separator.setAttribute("value", eval("\"\\u2022\"")); // bullet
          separator.setAttribute("class", "tag-separator");
          row.appendChild(separator);
        }
        var tag = this._createTagElement(childFolder, false);
        elementDict[childFolder.folderId] = tag;
        row.appendChild(tag);
      }
      childFolder.containerOpen = false;
    }
  },

  /**
   * This method creates a XUL element to represent a given Bookmark
   * folder node.
   *
   * @param aNode an nsINavHistoryFolderResultNode object
   * @param aIsSelected boolean, true if the given folder is currently selected
   *
   * @return a new XUL element corresponding to aNode
   */

  _createTagElement: function PBS_createTagElement(node, isSelected) {
    var tag = document.createElement("label");
    tag.setAttribute("value", node.title);
    tag.setAttribute("folderid", node.folderId);
    tag.setAttribute("selected", "" + isSelected);
    var self = this;
    function onClick(e) {
      self.tagClicked(e);
    }
    tag.addEventListener("command", onClick, false);
    // We need the click event handler until we change the element from labels
    // to something like checkboxes.
    tag.addEventListener("click", onClick, false);
    tag.setAttribute("class", "tag");
    return tag;
  },

  /**
   * This method should be called when a tag element generated by
   * _createTagElement is clicked by the user.
   */

  tagClicked: function PBS_tagClicked(event) {
    var tagElement = event.target;
    var folderId = parseInt(tagElement.getAttribute("folderid"));

    if (tagElement.getAttribute("selected") == "true") {
      this._bms.removeItem(folderId, this._currentURI);
      tagElement.setAttribute("selected", "false");
    } else {
      this._bms.insertItem(folderId, this._currentURI, -1);
      tagElement.setAttribute("selected", "true");
    }

    this._updateFolderTextbox(this._currentURI);
  },

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
