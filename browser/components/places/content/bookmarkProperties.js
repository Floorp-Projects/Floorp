/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Bookmark Properties.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hughes <jhughes@google.com>
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Asaf Romano <mano@mozilla.com>
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
 * The panel is initialized based on data given in the js object passed
 * as window.arguments[0]. The object must have the following fields set:
 *   @ action (String). Possible values:
 *     - "add" - for adding a new item.
 *       @ type (String). Possible values:
 *         - "bookmark"
 *         - "folder"
 *         - "folder with items"
 *           @ URIList (Array of nsIURI objects)- list of uris to be bookmarked
 *             under the new folder.
 *         - "livemark"
 *       @ uri (nsIURI object) - optional, the default uri for the new item.
 *         The property is not used for the "folder with items" type.
 *       @ title (String) - optional, the defualt title for the new item.
 *       @ defaultInsertionPoint (InsertionPoint JS object) - optional, the
 *         default insertion point for the new item.
 *      Notes:
 *        1) If |uri| is set for a bookmark/livemark item and |title| isn't,
 *           the dialog will query the history tables for the title associated
 *           with the given uri. For "folder with items" folder, a default
 *           static title is used ("[Folder Name]").
 *        2) The index field of the the default insertion point is ignored if
 *           the folder picker is shown.
 *     - "edit" - for editing a bookmark item or a folder.
 *       @ type (String). Possible values:
 *         - "bookmark"
 *           @ bookmarkId (Integer) - the id of the bookmark item.
 *         - "folder" (also applies to livemarks)
 *           @ folderId (Integer) - the id of the folder.
 *   @ hiddenRows (Strings array) - optional, list of rows to be hidden
 *     regardless of the item edited or added by the dialog.
 *     Possible values:
 *     - "title"
 *     - "location"
 *     - "description" (XXXmano: not yet implemented)
 *     - "keyword"
 *     - "microsummary"
 *     - "show in sidebar" (XXXmano: not yet implemented)
 *     - "feedURI"
 *     - "siteURI"
 *     - "folder picker" - hides both the tree and the menu.
 *
 * window.arguments[0].performed is set to true if any transaction has
 * been performed by the dialog.
 */

const BOOKMARK_ITEM = 0;
const BOOKMARK_FOLDER = 1;
const LIVEMARK_CONTAINER = 2;

const ACTION_EDIT = 0;
const ACTION_ADD = 1;
const ACTION_ADD_WITH_ITEMS = 2;

/**
 * Supported options:
 * BOOKMARK_ITEM : ACTION_EDIT, ACTION_ADD
 * BOOKMARK_FOLDER : ACTION_EDIT, ADD_WITH_ITEMS
 * LIVEMARK_CONTAINER : ACTION_EDIT
 */

var BookmarkPropertiesPanel = {

  /** UI Text Strings */
  __strings: null,
  get _strings() {
    if (!this.__strings) {
      this.__strings = document.getElementById("stringBundle");
    }
    return this.__strings;
  },

  /**
   * The Microsummary Service for displaying microsummaries.
   */
  __mss: null,
  get _mss() {
    if (!this.__mss)
      this.__mss = Cc["@mozilla.org/microsummary/service;1"].
                  getService(Ci.nsIMicrosummaryService);
    return this.__mss;
  },

  _action: null,
  _itemType: null,
  _folderId: null,
  _bookmarkId: null,
  _bookmarkURI: null,
  _itemTitle: "",
  _microsummaries: null,

  /**
   * This method returns the correct label for the dialog's "accept"
   * button based on the variant of the dialog.
   */
  _getAcceptLabel: function BPP__getAcceptLabel() {
    if (this._action == ACTION_ADD)
      return this._strings.getString("dialogAcceptLabelAdd");
    if (this._action == ACTION_ADD_WITH_ITEMS)
      return this._strings.getString("dialogAcceptLabelAddMulti");

    return this._strings.getString("dialogAcceptLabelEdit");
  },

  /**
   * This method returns the correct title for the current variant
   * of this dialog.
   */
  _getDialogTitle: function BPP__getDialogTitle() {
    if (this._action == ACTION_ADD) {
      if (this._itemType == BOOKMARK_ITEM)
        return this._strings.getString("dialogTitleAddBookmark");
      if (this._itemType == LIVEMARK_CONTAINER)
        return this._strings.getString("dialogTitleAddLivemark");

      // folder
      NS_ASSERT(this._itemType == BOOKMARK_FOLDER, "bogus item type");
      return this._strings.getString("dialogTitleAddFolder");
    }
    if (this._action == ACTION_ADD_WITH_ITEMS)
      return this._strings.getString("dialogTitleAddMulti");
    if (this._action == ACTION_EDIT) {
      return this._strings
                 .getFormattedString("dialogTitleEdit", [this._itemTitle]);
    }
  },

  /**
   * Determines the initial data for the item edited or added by this dialog
   */
  _determineItemInfo: function BPP__determineItemInfo() {
    var dialogInfo = window.arguments[0];
    NS_ASSERT("action" in dialogInfo, "missing action property");
    var action = dialogInfo.action;

    if (action == "add") {
      NS_ASSERT("type" in dialogInfo, "missing type property for add action");

      if ("title" in dialogInfo)
        this._itemTitle = dialogInfo.title;
      if ("defaultInsertionPoint" in dialogInfo)
        this._defaultInsertionPoint = dialogInfo.defaultInsertionPoint;

      switch(dialogInfo.type) {
        case "bookmark":
          this._action = ACTION_ADD;
          this._itemType = BOOKMARK_ITEM;
          if ("uri" in dialogInfo) {
            NS_ASSERT(dialogInfo.uri instanceof Ci.nsIURI,
                      "uri property should be a uri object");
            this._bookmarkURI = dialogInfo.uri;
          }
          if (!this._itemTitle) {
            if (this._bookmarkURI) {
              this._itemTitle =
                this._getURITitleFromHistory(this._bookmarkURI);
              if (!this._itemTitle)
                this._itemTitle = this._bookmarkURI.spec;
            }
            else
              this._itemTitle = this._strings.getString("newBookmarkDefault");
          }
          break;
        case "folder":
          this._action = ACTION_ADD;
          this._itemType = BOOKMARK_FOLDER;
          if (!this._itemTitle)
            this._itemTitle = this._strings.getString("newFolderDefault");
          break;
        case "folder with items":
          NS_ASSERT("URIList" in dialogInfo,
                    "missing URLList property for 'folder with items' action");
          this._action = ACTION_ADD_WITH_ITEMS
          this._itemType = BOOKMARK_FOLDER;
          this._URIList = dialogInfo.URIList;
          if (!this._itemTitle) {
            this._itemTitle =
              this._strings.getString("bookmarkAllTabsDefault");
          }
          break;
        case "livemark":
          this._action = ACTION_ADD;
          this._itemType = LIVEMARK_CONTAINER;
          if ("feedURI" in dialogInfo)
            this._feedURI = dialogInfo.feedURI;
          if ("siteURI" in dialogInfo)
            this._siteURI = dialogInfo.siteURI;

          if (!this._itemTitle) {
            if (this._feedURI) {
              this._itemTitle =
                this._getURITitleFromHistory(this._feedURI);
              if (!this._itemTitle)
                this._itemTitle = this._feedURI.spec;
            }
            else
              this._itemTitle = this._strings.getString("newLivemarkDefault");
          }
      }
    }
    else { // edit
      switch (dialogInfo.type) {
        case "bookmark":
          NS_ASSERT("bookmarkId" in dialogInfo);
          this._action = ACTION_EDIT;
          this._itemType = BOOKMARK_ITEM;
          this._bookmarkId = dialogInfo.bookmarkId;
          this._bookmarkURI =
            PlacesUtils.bookmarks.getBookmarkURI(this._bookmarkId);
          this._itemTitle = PlacesUtils.bookmarks
                                           .getItemTitle(this._bookmarkId);
          this._bookmarkKeyword =
            PlacesUtils.bookmarks.getKeywordForBookmark(this._bookmarkId);
          break;
        case "folder":
          NS_ASSERT("folderId" in dialogInfo);
          this._action = ACTION_EDIT;
          this._folderId = dialogInfo.folderId;
          if (PlacesUtils.livemarks.isLivemark(this._folderId)) {
            this._itemType = LIVEMARK_CONTAINER;
            this._feedURI = PlacesUtils.livemarks.getFeedURI(this._folderId);
            this._siteURI = PlacesUtils.livemarks.getSiteURI(this._folderId);
          }
          else
            this._itemType = BOOKMARK_FOLDER;
          this._itemTitle =
            PlacesUtils.bookmarks.getFolderTitle(this._folderId);
          break;
      }
    }
  },

  /**
   * This method returns the title string corresponding to a given URI.
   * If none is available from the bookmark service (probably because
   * the given URI doesn't appear in bookmarks or history), we synthesize
   * a title from the first 100 characters of the URI.
   *
   * @param aURI
   *        nsIURI object for which we want the title
   *
   * @returns a title string
   */

  _getURITitleFromHistory: function BPP__getURITitleFromHistory(aURI) {
    NS_ASSERT(aURI instanceof Ci.nsIURI);

    // get the title from History
    return PlacesUtils.history.getPageTitle(aURI);
  },

  /**
   * This method should be called by the onload of the Bookmark Properties
   * dialog to initialize the state of the panel.
   */
  onDialogLoad: function BPP_onDialogLoad() {
    this._tm = window.opener.PlacesUtils.tm;

    this._determineItemInfo();
    this._initFolderTree();
    this._populateProperties();
    this._forceHideRows();
    this.validateChanges();
    this._updateSize();
  },

  /**
   * This method initializes the folder tree.
   */
  _initFolderTree: function BPP__initFolderTree() {
    this._folderTree = this._element("folderTree");
    this._folderTree.peerDropTypes = [];
    this._folderTree.childDropTypes = [];
    if (isElementVisible(this._folderTree)) {
      if (this._defaultInsertionPoint)
        this._folderTree.selectFolders([this._defaultInsertionPoint.folderId]);
      else
        this._folderTree.selectFolders([PlacesUtils.bookmarks.bookmarksRoot]);
    }
  },

  _initMicrosummaryPicker: function BPP__initMicrosummaryPicker() {
    try {
      this._microsummaries = this._mss.getMicrosummaries(this._bookmarkURI,
                                                         this._bookmarkURI);
    }
    catch(ex) {
      // There was a problem retrieving microsummaries; disable the picker.
      // The microsummary service will throw an exception in at least
      // two cases:
      // 1. the bookmarked URI contains a scheme that the service won't
      //    download for security reasons (currently it only handles http,
      //    https, and file);
      // 2. the page to which the URI refers isn't HTML or XML (the only two
      //    content types the service knows how to summarize).
      this._element("microsummaryRow").hidden = true;
      return;
    }
    this._microsummaries.addObserver(this._microsummaryObserver);
    this._rebuildMicrosummaryPicker();
  },

  _element: function BPP__element(aID) {
    return document.getElementById(aID);
  },

  /**
   * Hides fields which were explicitly set hidden by the the dialog opener
   * (see documentation at the top of this file).
   */
  _forceHideRows: function BPP__forceHideRows() {
    var hiddenRows = window.arguments[0].hiddenRows;
    if (!hiddenRows)
      return;

    if (hiddenRows.indexOf("title")!= -1)
      this._element("titleTextfield").hidden = true;
    if (hiddenRows.indexOf("location")!= -1)
      this._element("locationRow").hidden = true;
    if (hiddenRows.indexOf("keyword")!= -1)
      this._element("shortcutRow").hidden = true;
    if (hiddenRows.indexOf("folder picker") != -1)
      this._element("folderRow").hidden = true;
    if (hiddenRows.indexOf("feedURI") != -1)
      this._element("livemarkFeedLocationRow").hidden = true;
    if (hiddenRows.indexOf("siteURI") != -1)
      this._element("livemarkSiteLocationRow").hidden = true;
    if (hiddenRows.indexOf("microsummary") != -1)
      this._element("microsummaryRow").hidden = true;
  },

  /**
   * This method fills in the data values for the fields in the dialog.
   */
  _populateProperties: function BPP__populateProperties() {
    document.title = this._getDialogTitle();
    document.documentElement.getButton("accept").label = this._getAcceptLabel();
    if (this._itemTitle)
      this._element("titleTextfield").value = this._itemTitle;

    if (this._itemType == BOOKMARK_ITEM) {
      if (this._bookmarkURI)
        this._element("editURLBar").value = this._bookmarkURI.spec;

      if (this._bookmarkKeyword)
        this._element("keywordTextfield").value = this._bookmarkKeyword;
    }
    else {
      this._element("locationRow").hidden = true;
      this._element("shortcutRow").hidden = true;
    }

    if (this._itemType == LIVEMARK_CONTAINER) {
      if (this._feedURI)
        this._element("feedLocationTextfield").value = this._feedURI.spec;
      if (this._siteURI)
        this._element("feedSiteLocationTextfield").value = this._siteURI.spec;
    }
    else {
      this._element("livemarkFeedLocationRow").hidden = true;
      this._element("livemarkSiteLocationRow").hidden = true;
    }

    if (this._itemType == BOOKMARK_ITEM && this._bookmarkURI) {
      // _initMicrosummaryPicker may also hide the row
      this._initMicrosummaryPicker();
    }
    else
      this._element("microsummaryRow").hidden = true;

    if (this._action == ACTION_EDIT)
      this._element("folderRow").hidden = true;
  },

  //XXXDietrich - bug 370215 - update to use bookmark id once 360133 is fixed.
  _rebuildMicrosummaryPicker: function BPP__rebuildMicrosummaryPicker() {
    var microsummaryMenuList = this._element("microsummaryMenuList");
    var microsummaryMenuPopup = this._element("microsummaryMenuPopup");

    // Remove old items from the menu, except the first item, which represents
    // "don't show a microsummary; show the page title instead".
    while (microsummaryMenuPopup.childNodes.length > 1)
      microsummaryMenuPopup.removeChild(microsummaryMenuPopup.lastChild);

    var enumerator = this._microsummaries.Enumerate();
    while (enumerator.hasMoreElements()) {
      var microsummary = enumerator.getNext().QueryInterface(Ci.nsIMicrosummary);

      var menuItem = document.createElement("menuitem");

      // Store a reference to the microsummary in the menu item, so we know
      // which microsummary this menu item represents when it's time to save
      // changes to the datastore.
      menuItem.microsummary = microsummary;

      // Content may have to be generated asynchronously; we don't necessarily
      // have it now.  If we do, great; otherwise, fall back to the generator
      // name, then the URI, and we trigger a microsummary content update.
      // Once the update completes, the microsummary will notify our observer
      // to rebuild the menu.
      // XXX Instead of just showing the generator name or (heaven forbid)
      // its URI when we don't have content, we should tell the user that we're
      // loading the microsummary, perhaps with some throbbing to let her know
      // it's in progress.
      if (microsummary.content)
        menuItem.setAttribute("label", microsummary.content);
      else {
        menuItem.setAttribute("label", microsummary.generator ?
                                       microsummary.generator.name :
                                       microsummary.generatorURI.spec);
        microsummary.update();
      }

      microsummaryMenuPopup.appendChild(menuItem);

      // Select the item if this is the current microsummary for the bookmark.
      if (this._mss.isMicrosummary(this._bookmarkURI, microsummary))
        microsummaryMenuList.selectedItem = menuItem;
    }
  },

  _microsummaryObserver: {
    QueryInterface: function (aIID) {
      if (!aIID.equals(Ci.nsIMicrosummaryObserver) &&
          !aIID.equals(Ci.nsISupports))
        throw Cr.NS_ERROR_NO_INTERFACE;
      return this;
    },

    onContentLoaded: function(aMicrosummary) {
      BookmarkPropertiesPanel._rebuildMicrosummaryPicker();
    },

    onElementAppended: function(aMicrosummary) {
      BookmarkPropertiesPanel._rebuildMicrosummaryPicker();
    }
  },

  /**
   * Size the dialog to fit its contents.
   */
  _updateSize: function BPP__updateSize() {
    var width = window.outerWidth;
    window.sizeToContent();
    window.resizeTo(width, window.outerHeight);
  },

  onDialogUnload: function BPP_onDialogUnload() {
    if (this._microsummaries)
      this._microsummaries.removeObserver(this._microsummaryObserver);
  },

  onDialogAccept: function BPP_onDialogAccept() {
    this._saveChanges();
  },

  /**
   * This method checks the current state of the input fields in the
   * dialog, and if any of them are in an invalid state, it will disable
   * the submit button.  This method should be called after every
   * significant change to the input.
   */
  validateChanges: function BPP_validateChanges() {
    document.documentElement.getButton("accept").disabled = !this._inputIsValid();
  },

  /**
   * This method checks to see if the input fields are in a valid state.
   *
   * @returns  true if the input is valid, false otherwise
   */
  _inputIsValid: function BPP__inputIsValid() {
    if (this._itemType == BOOKMARK_ITEM && !this._containsValidURI("editURLBar"))
      return false;

    // Feed Location has to be a valid URI;
    // Site Location has to be a valid URI or empty
    if (this._itemType == LIVEMARK_CONTAINER) {
      if (!this._containsValidURI("feedLocationTextfield"))
        return false;
      if (!this._containsValidURI("feedSiteLocationTextfield") &&
          (this._element("feedSiteLocationTextfield").value.length > 0))
        return false;
    }

    return true;
  },

  /**
   * Determines whether the XUL textbox with the given ID contains a
   * string that can be converted into an nsIURI.
   *
   * @param aTextboxID
   *        the ID of the textbox element whose contents we'll test
   *
   * @returns true if the textbox contains a valid URI string, false otherwise
   */
  _containsValidURI: function BPP__containsValidURI(aTextboxID) {
    try {
      var value = this._element(aTextboxID).value;
      if (value) {
        var uri = PlacesUtils._uri(value);
        return true;
      }
    } catch (e) { }
    return false;
  },

  /**
   * Get an edit title transaction for the item edit/added in the dialog
   */
  _getEditTitleTransaction:
  function BPP__getEditTitleTransaction(aItemId, aNewTitle) {
    // XXXmano: remove this once bug 372508 is fixed
    if (this._itemType == BOOKMARK_ITEM)
      return new PlacesEditItemTitleTransaction(aItemId, aNewTitle);

    // folder or livemark container
    return new PlacesEditFolderTitleTransaction(aItemId, aNewTitle);
  },

  /**
   * Get a create-item transaction for the item added in the dialog
   */
  _getCreateItemTransaction: function() {
    NS_ASSERT(this._action != ACTION_EDIT,
              "_getCreateItemTransaction called when editing an item");

    var containerId, indexInContainer = -1;
    if (isElementVisible(this._folderTree))
      containerId =  asFolder(this._folderTree.selectedNode).folderId;
    else if (this._defaultInsertionPoint) {
      containerId = this._defaultInsertionPoint.folderId;
      indexInContainer = this._defaultInsertionPoint.index;
    }
    else
      containerId = PlacesUtils.bookmarks.bookmarksRoot;

    if (this._itemType == BOOKMARK_ITEM) {
      var uri = PlacesUtils._uri(this._element("editURLBar").value);
      NS_ASSERT(uri, "cannot create an item without a uri");
      return new
        PlacesCreateItemTransaction(uri, containerId, indexInContainer);
    }
    else if (this._itemType == LIVEMARK_CONTAINER) {
      var feedURIString = this._element("feedLocationTextfield").value;
      var feedURI = PlacesUtils._uri(feedURIString);

      var siteURIString = this._element("feedSiteLocationTextfield").value;
      var siteURI = null;
      if (siteURIString)
        siteURI = PlacesUtils._uri(siteURIString);

      var name = this._element("titleTextfield").value;

      return new PlacesCreateLivemarkTransaction(feedURI, siteURI,
                                                 name, containerId,
                                                 indexInContainer);
    }
    else if (this._itemType == BOOKMARK_FOLDER) { // folder
      var name = this._element("titleTextfield").value;
      return new PlacesCreateFolderTransaction(name, containerId,
                                               indexInContainer);
    }
  },

  /**
   * Save any changes that might have been made while the properties dialog
   * was open.
   */
  _saveChanges: function BPP__saveChanges() {
    var transactions = [];
    var childItemsTransactions = [];

    // -1 is used when creating a new item (for child transactions).
    var itemId = -1;
    if (this._action == ACTION_EDIT) {
      if (this._itemType == BOOKMARK_ITEM)
        itemId = this._bookmarkId;
      else
        itemId = this._folderId;
    }

    // for ACTION_ADD, the uri is set via insertItem
    if (this._action == ACTION_EDIT && this._itemType == BOOKMARK_ITEM) {
      var url = PlacesUtils._uri(this._element("editURLBar").value);
      if (!this._bookmarkURI.equals(url))
        transactions.push(new PlacesEditBookmarkURITransaction(itemId, url));
    }

    // title transaction
    // XXXmano: this isn't necessary for new folders. We should probably
    // make insertItem take a title too (like insertFolder)
    var newTitle = this._element("titleTextfield").value;
    if (this._action != ACTION_EDIT || newTitle != this._itemTitle)
      transactions.push(this._getEditTitleTransaction(itemId, newTitle));

    // keyword transactions
    if (this._itemType == BOOKMARK_ITEM) {
      var newKeyword = this._element("keywordTextfield").value;
      if (this._action != ACTION_EDIT || newKeyword != this._bookmarkKeyword) {
        transactions.push(
          new PlacesEditBookmarkKeywordTransaction(itemId, newKeyword));
      }
    }

    // items under a new folder
    if (this._action == ACTION_ADD_WITH_ITEMS) {
      for (var i = 0; i < this._URIList.length; ++i) {
        var uri = this._URIList[i];
        var title = this._getURITitleFromHistory(uri);
        var txn = new PlacesCreateItemTransaction(uri, -1, -1);
        txn.childTransactions.push(
          new PlacesEditItemTitleTransaction(-1, title));
        childItemsTransactions.push(txn);
      }
    }

    // See _getCreateItemTransaction for the add action case
    if (this._action == ACTION_EDIT && this._itemType == LIVEMARK_CONTAINER) {
      var feedURIString = this._element("feedLocationTextfield").value;
      var feedURI = PlacesUtils._uri(feedURIString);
      if (!this._feedURI.equals(feedURI)) {
        transactions.push(
          new PlacesEditLivemarkFeedURITransaction(this._folderId, feedURI));
      }

      // Site Location is empty, we can set its URI to null
      var siteURIString = this._element("feedSiteLocationTextfield").value;
      var siteURI = null;
      if (siteURIString)
        siteURI = PlacesUtils._uri(siteURIString);

      if ((!siteURI && this._siteURI)  ||
          (siteURI && !this._siteURI.equals(siteURI))) {
        transactions.push(
          new PlacesEditLivemarkSiteURITransaction(this._folderId, siteURI));
      }
    }

    // microsummaries
    var menuList = this._element("microsummaryMenuList");
    if (isElementVisible(menuList)) {
      // Something should always be selected in the microsummary menu,
      // but if nothing is selected, then conservatively assume we should
      // just display the bookmark title.
      if (menuList.selectedIndex == -1)
        menuList.selectedIndex = 0;

      // This will set microsummary == undefined if the user selected
      // the "don't display a microsummary" item.
      var newMicrosummary = menuList.selectedItem.microsummary;

      // Only add a microsummary update to the transaction if the microsummary
      // has actually changed, i.e. the user selected no microsummary,
      // but the bookmark previously had one, or the user selected a microsummary
      // which is not the one the bookmark previously had.
      if ((newMicrosummary == null &&
           this._mss.hasMicrosummary(this._bookmarkURI)) ||
          (newMicrosummary != null &&
           !this._mss.isMicrosummary(this._bookmarkURI, newMicrosummary))) {
        transactions.push(
          new PlacesEditBookmarkMicrosummaryTransaction(itemId,
                                                        newMicrosummary));
      }
    }

    // If we have any changes to perform, do them via the
    // transaction manager passed by the opener so they can be undone.
    if (transactions.length > 0) {
      window.arguments[0].performed = true;

      if (this._action != ACTION_EDIT) {
        var createTxn = this._getCreateItemTransaction();
        NS_ASSERT(createTxn, "failed to get a create-item transaction");

        // use child transactions if we're creating a new item
        createTxn.childTransactions =
          createTxn.childTransactions.concat(transactions);

        if (this._action == ACTION_ADD_WITH_ITEMS) {
          // use child-items transactions for creating items under the root item
          createTxn.childItemsTransactions =
            createTxn.childItemsTransactions.concat(childItemsTransactions);
        }
        this._tm.doTransaction(createTxn);
      }
      else {
        // just aggregate otherwise
        var aggregate =
          new PlacesAggregateTransaction(this._getDialogTitle(), transactions);
        this._tm.doTransaction(aggregate);
      }
    }
  }
};
