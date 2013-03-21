/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The panel is initialized based on data given in the js object passed
 * as window.arguments[0]. The object must have the following fields set:
 *   @ action (String). Possible values:
 *     - "add" - for adding a new item.
 *       @ type (String). Possible values:
 *         - "bookmark"
 *           @ loadBookmarkInSidebar - optional, the default state for the
 *             "Load this bookmark in the sidebar" field.
 *         - "folder"
 *           @ URIList (Array of nsIURI objects) - optional, list of uris to
 *             be bookmarked under the new folder.
 *         - "livemark"
 *       @ uri (nsIURI object) - optional, the default uri for the new item.
 *         The property is not used for the "folder with items" type.
 *       @ title (String) - optional, the default title for the new item.
 *       @ description (String) - optional, the default description for the new
 *         item.
 *       @ defaultInsertionPoint (InsertionPoint JS object) - optional, the
 *         default insertion point for the new item.
 *       @ keyword (String) - optional, the default keyword for the new item.
 *       @ postData (String) - optional, POST data to accompany the keyword.
 *       @ charSet (String) - optional, character-set to accompany the keyword.
 *      Notes:
 *        1) If |uri| is set for a bookmark/livemark item and |title| isn't,
 *           the dialog will query the history tables for the title associated
 *           with the given uri. If the dialog is set to adding a folder with
 *           bookmark items under it (see URIList), a default static title is
 *           used ("[Folder Name]").
 *        2) The index field of the default insertion point is ignored if
 *           the folder picker is shown.
 *     - "edit" - for editing a bookmark item or a folder.
 *       @ type (String). Possible values:
 *         - "bookmark"
 *           @ itemId (Integer) - the id of the bookmark item.
 *         - "folder" (also applies to livemarks)
 *           @ itemId (Integer) - the id of the folder.
 *   @ hiddenRows (Strings array) - optional, list of rows to be hidden
 *     regardless of the item edited or added by the dialog.
 *     Possible values:
 *     - "title"
 *     - "location"
 *     - "description"
 *     - "keyword"
 *     - "tags"
 *     - "loadInSidebar"
 *     - "feedLocation"
 *     - "siteLocation"
 *     - "folderPicker" - hides both the tree and the menu.
 *   @ readOnly (Boolean) - optional, states if the panel should be read-only
 *
 * window.arguments[0].performed is set to true if any transaction has
 * been performed by the dialog.
 */

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

const BOOKMARK_ITEM = 0;
const BOOKMARK_FOLDER = 1;
const LIVEMARK_CONTAINER = 2;

const ACTION_EDIT = 0;
const ACTION_ADD = 1;

var BookmarkPropertiesPanel = {

  /** UI Text Strings */
  __strings: null,
  get _strings() {
    if (!this.__strings) {
      this.__strings = document.getElementById("stringBundle");
    }
    return this.__strings;
  },

  _action: null,
  _itemType: null,
  _itemId: -1,
  _uri: null,
  _loadInSidebar: false,
  _title: "",
  _description: "",
  _URIs: [],
  _keyword: "",
  _postData: null,
  _charSet: "",
  _feedURI: null,
  _siteURI: null,

  _defaultInsertionPoint: null,
  _hiddenRows: [],
  _batching: false,
  _readOnly: false,

  /**
   * This method returns the correct label for the dialog's "accept"
   * button based on the variant of the dialog.
   */
  _getAcceptLabel: function BPP__getAcceptLabel() {
    if (this._action == ACTION_ADD) {
      if (this._URIs.length)
        return this._strings.getString("dialogAcceptLabelAddMulti");

      if (this._itemType == LIVEMARK_CONTAINER)
        return this._strings.getString("dialogAcceptLabelAddLivemark");

      if (this._dummyItem || this._loadInSidebar)
        return this._strings.getString("dialogAcceptLabelAddItem");

      return this._strings.getString("dialogAcceptLabelSaveItem");
    }
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

      // add folder
      NS_ASSERT(this._itemType == BOOKMARK_FOLDER, "Unknown item type");
      if (this._URIs.length)
        return this._strings.getString("dialogTitleAddMulti");

      return this._strings.getString("dialogTitleAddFolder");
    }
    if (this._action == ACTION_EDIT) {
      return this._strings.getFormattedString("dialogTitleEdit", [this._title]);
    }
    return "";
  },

  /**
   * Determines the initial data for the item edited or added by this dialog
   */
  _determineItemInfo: function BPP__determineItemInfo() {
    var dialogInfo = window.arguments[0];
    this._action = dialogInfo.action == "add" ? ACTION_ADD : ACTION_EDIT;
    this._hiddenRows = dialogInfo.hiddenRows ? dialogInfo.hiddenRows : [];
    if (this._action == ACTION_ADD) {
      NS_ASSERT("type" in dialogInfo, "missing type property for add action");

      if ("title" in dialogInfo)
        this._title = dialogInfo.title;

      if ("defaultInsertionPoint" in dialogInfo) {
        this._defaultInsertionPoint = dialogInfo.defaultInsertionPoint;
      }
      else
        this._defaultInsertionPoint =
          new InsertionPoint(PlacesUtils.bookmarksMenuFolderId,
                             PlacesUtils.bookmarks.DEFAULT_INDEX,
                             Ci.nsITreeView.DROP_ON);

      switch (dialogInfo.type) {
        case "bookmark":
          this._itemType = BOOKMARK_ITEM;
          if ("uri" in dialogInfo) {
            NS_ASSERT(dialogInfo.uri instanceof Ci.nsIURI,
                      "uri property should be a uri object");
            this._uri = dialogInfo.uri;
            if (typeof(this._title) != "string") {
              this._title = this._getURITitleFromHistory(this._uri) ||
                            this._uri.spec;
            }
          }
          else {
            this._uri = PlacesUtils._uri("about:blank");
            this._title = this._strings.getString("newBookmarkDefault");
            this._dummyItem = true;
          }

          if ("loadBookmarkInSidebar" in dialogInfo)
            this._loadInSidebar = dialogInfo.loadBookmarkInSidebar;

          if ("keyword" in dialogInfo) {
            this._keyword = dialogInfo.keyword;
            this._isAddKeywordDialog = true;
            if ("postData" in dialogInfo)
              this._postData = dialogInfo.postData;
            if ("charSet" in dialogInfo)
              this._charSet = dialogInfo.charSet;
          }
          break;

        case "folder":
          this._itemType = BOOKMARK_FOLDER;
          if (!this._title) {
            if ("URIList" in dialogInfo) {
              this._title = this._strings.getString("bookmarkAllTabsDefault");
              this._URIs = dialogInfo.URIList;
            }
            else
              this._title = this._strings.getString("newFolderDefault");
              this._dummyItem = true;
          }
          break;

        case "livemark":
          this._itemType = LIVEMARK_CONTAINER;
          if ("feedURI" in dialogInfo)
            this._feedURI = dialogInfo.feedURI;
          if ("siteURI" in dialogInfo)
            this._siteURI = dialogInfo.siteURI;

          if (!this._title) {
            if (this._feedURI) {
              this._title = this._getURITitleFromHistory(this._feedURI) ||
                            this._feedURI.spec;
            }
            else
              this._title = this._strings.getString("newLivemarkDefault");
          }
      }

      if ("description" in dialogInfo)
        this._description = dialogInfo.description;
    }
    else { // edit
      NS_ASSERT("itemId" in dialogInfo);
      this._itemId = dialogInfo.itemId;
      this._title = PlacesUtils.bookmarks.getItemTitle(this._itemId);
      this._readOnly = !!dialogInfo.readOnly;

      switch (dialogInfo.type) {
        case "bookmark":
          this._itemType = BOOKMARK_ITEM;

          this._uri = PlacesUtils.bookmarks.getBookmarkURI(this._itemId);
          // keyword
          this._keyword = PlacesUtils.bookmarks
                                     .getKeywordForBookmark(this._itemId);
          // Load In Sidebar
          this._loadInSidebar = PlacesUtils.annotations
                                           .itemHasAnnotation(this._itemId,
                                                              PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO);
          break;

        case "folder":
          this._itemType = BOOKMARK_FOLDER;
          PlacesUtils.livemarks.getLivemark(
            { id: this._itemId },
            (function (aStatus, aLivemark) {
              if (Components.isSuccessCode(aStatus)) {
                this._itemType = LIVEMARK_CONTAINER;
                this._feedURI = aLivemark.feedURI;
                this._siteURI = aLivemark.siteURI;
                this._fillEditProperties();

                let acceptButton = document.documentElement.getButton("accept");
                acceptButton.disabled = !this._inputIsValid();

                let newHeight = window.outerHeight +
                                this._element("descriptionField").boxObject.height;
                window.resizeTo(window.outerWidth, newHeight);
              }
            }).bind(this)
          );

          break;
      }

      // Description
      if (PlacesUtils.annotations
                     .itemHasAnnotation(this._itemId, PlacesUIUtils.DESCRIPTION_ANNO)) {
        this._description = PlacesUtils.annotations
                                       .getItemAnnotation(this._itemId,
                                                          PlacesUIUtils.DESCRIPTION_ANNO);
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
    this._determineItemInfo();

    document.title = this._getDialogTitle();
    var acceptButton = document.documentElement.getButton("accept");
    acceptButton.label = this._getAcceptLabel();

    this._beginBatch();

    switch (this._action) {
      case ACTION_EDIT:
        this._fillEditProperties();
        acceptButton.disabled = this._readOnly;
        break;
      case ACTION_ADD:
        this._fillAddProperties();
        // if this is an uri related dialog disable accept button until
        // the user fills an uri value.
        if (this._itemType == BOOKMARK_ITEM)
          acceptButton.disabled = !this._inputIsValid();
        break;
    }

    // When collapsible elements change their collapsed attribute we must
    // resize the dialog.
    // sizeToContent is not usable due to bug 90276, so we'll use resizeTo
    // instead and cache the element size. See WSucks in the legacy
    // UI code (addBookmark2.js).
    if (!this._element("tagsRow").collapsed) {
      this._element("tagsSelectorRow")
          .addEventListener("DOMAttrModified", this, false);
    }
    if (!this._element("folderRow").collapsed) {
      this._element("folderTreeRow")
          .addEventListener("DOMAttrModified", this, false);
    }

    if (!this._readOnly) {
      // Listen on uri fields to enable accept button if input is valid
      if (this._itemType == BOOKMARK_ITEM) {
        this._element("locationField")
            .addEventListener("input", this, false);
        if (this._isAddKeywordDialog) {
          this._element("keywordField")
              .addEventListener("input", this, false);
        }
      }
      else if (this._itemType == LIVEMARK_CONTAINER) {
        this._element("feedLocationField")
            .addEventListener("input", this, false);
        this._element("siteLocationField")
            .addEventListener("input", this, false);
      }
    }

    window.sizeToContent();
  },

  // nsIDOMEventListener
  _elementsHeight: [],
  handleEvent: function BPP_handleEvent(aEvent) {
    var target = aEvent.target;
    switch (aEvent.type) {
      case "input":
        if (target.id == "editBMPanel_locationField" ||
            target.id == "editBMPanel_feedLocationField" ||
            target.id == "editBMPanel_siteLocationField" ||
            target.id == "editBMPanel_keywordField") {
          // Check uri fields to enable accept button if input is valid
          document.documentElement
                  .getButton("accept").disabled = !this._inputIsValid();
        }
        break;

      case "DOMAttrModified":
        // this is called when collapsing a node, but also its direct children,
        // we only need to resize when the original node changes.
        if ((target.id == "editBMPanel_tagsSelectorRow" ||
             target.id == "editBMPanel_folderTreeRow") &&
            aEvent.attrName == "collapsed" &&
            target == aEvent.originalTarget) {
          var id = target.id;
          var newHeight = window.outerHeight;
          if (aEvent.newValue) // is collapsed
            newHeight -= this._elementsHeight[id];
          else {
            this._elementsHeight[id] = target.boxObject.height;
            newHeight += this._elementsHeight[id];
          }

          window.resizeTo(window.outerWidth, newHeight);
        }
        break;
    }
  },

  _beginBatch: function BPP__beginBatch() {
    if (this._batching)
      return;

    PlacesUtils.transactionManager.beginBatch(null);
    this._batching = true;
  },

  _endBatch: function BPP__endBatch() {
    if (!this._batching)
      return;

    PlacesUtils.transactionManager.endBatch(false);
    this._batching = false;
  },

  _fillEditProperties: function BPP__fillEditProperties() {
    gEditItemOverlay.initPanel(this._itemId,
                               { hiddenRows: this._hiddenRows,
                                 forceReadOnly: this._readOnly });
  },

  _fillAddProperties: function BPP__fillAddProperties() {
    this._createNewItem();
    // Edit the new item
    gEditItemOverlay.initPanel(this._itemId,
                               { hiddenRows: this._hiddenRows });
    // Empty location field if the uri is about:blank, this way inserting a new
    // url will be easier for the user, Accept button will be automatically
    // disabled by the input listener until the user fills the field.
    var locationField = this._element("locationField");
    if (locationField.value == "about:blank")
      locationField.value = "";
  },

  // nsISupports
  QueryInterface: function BPP_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIDOMEventListener) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_NOINTERFACE;
  },

  _element: function BPP__element(aID) {
    return document.getElementById("editBMPanel_" + aID);
  },

  onDialogUnload: function BPP_onDialogUnload() {
    // gEditItemOverlay does not exist anymore here, so don't rely on it.
    // Calling removeEventListener with arguments which do not identify any
    // currently registered EventListener on the EventTarget has no effect.
    this._element("tagsSelectorRow")
        .removeEventListener("DOMAttrModified", this, false);
    this._element("folderTreeRow")
        .removeEventListener("DOMAttrModified", this, false);
    this._element("locationField")
        .removeEventListener("input", this, false);
    this._element("feedLocationField")
        .removeEventListener("input", this, false);
    this._element("siteLocationField")
        .removeEventListener("input", this, false);
  },

  onDialogAccept: function BPP_onDialogAccept() {
    // We must blur current focused element to save its changes correctly
    document.commandDispatcher.focusedElement.blur();
    // The order here is important! We have to uninit the panel first, otherwise
    // late changes could force it to commit more transactions.
    gEditItemOverlay.uninitPanel(true);
    gEditItemOverlay = null;
    this._endBatch();
    window.arguments[0].performed = true;
  },

  onDialogCancel: function BPP_onDialogCancel() {
    // The order here is important! We have to uninit the panel first, otherwise
    // changes done as part of Undo may change the panel contents and by
    // that force it to commit more transactions.
    gEditItemOverlay.uninitPanel(true);
    gEditItemOverlay = null;
    this._endBatch();
    PlacesUtils.transactionManager.undoTransaction();
    window.arguments[0].performed = false;
  },

  /**
   * This method checks to see if the input fields are in a valid state.
   *
   * @returns  true if the input is valid, false otherwise
   */
  _inputIsValid: function BPP__inputIsValid() {
    if (this._itemType == BOOKMARK_ITEM &&
        !this._containsValidURI("locationField"))
      return false;
    if (this._isAddKeywordDialog && !this._element("keywordField").value.length)
      return false;

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
        PlacesUIUtils.createFixedURI(value);
        return true;
      }
    } catch (e) { }
    return false;
  },

  /**
   * [New Item Mode] Get the insertion point details for the new item, given
   * dialog state and opening arguments.
   *
   * The container-identifier and insertion-index are returned separately in
   * the form of [containerIdentifier, insertionIndex]
   */
  _getInsertionPointDetails: function BPP__getInsertionPointDetails() {
    var containerId = this._defaultInsertionPoint.itemId;
    var indexInContainer = this._defaultInsertionPoint.index;

    return [containerId, indexInContainer];
  },

  /**
   * Returns a transaction for creating a new bookmark item representing the
   * various fields and opening arguments of the dialog.
   */
  _getCreateNewBookmarkTransaction:
  function BPP__getCreateNewBookmarkTransaction(aContainer, aIndex) {
    var annotations = [];
    var childTransactions = [];

    if (this._description) {
      let annoObj = { name   : PlacesUIUtils.DESCRIPTION_ANNO,
                      type   : Ci.nsIAnnotationService.TYPE_STRING,
                      flags  : 0,
                      value  : this._description,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
      let editItemTxn = new PlacesSetItemAnnotationTransaction(-1, annoObj);
      childTransactions.push(editItemTxn);
    }

    if (this._loadInSidebar) {
      let annoObj = { name   : PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO,
                      type   : Ci.nsIAnnotationService.TYPE_INT32,
                      flags  : 0,
                      value  : this._loadInSidebar,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
      let setLoadTxn = new PlacesSetItemAnnotationTransaction(-1, annoObj);
      childTransactions.push(setLoadTxn);
    }

    if (this._postData) {
      let postDataTxn = new PlacesEditBookmarkPostDataTransaction(-1, this._postData);
      childTransactions.push(postDataTxn);
    }

    //XXX TODO: this should be in a transaction!
    if (this._charSet && !PrivateBrowsingUtils.isWindowPrivate(window))
      PlacesUtils.setCharsetForURI(this._uri, this._charSet);

    let createTxn = new PlacesCreateBookmarkTransaction(this._uri,
                                                        aContainer,
                                                        aIndex,
                                                        this._title,
                                                        this._keyword,
                                                        annotations,
                                                        childTransactions);

    return new PlacesAggregatedTransaction(this._getDialogTitle(),
                                           [createTxn]);
  },

  /**
   * Returns a childItems-transactions array representing the URIList with
   * which the dialog has been opened.
   */
  _getTransactionsForURIList: function BPP__getTransactionsForURIList() {
    var transactions = [];
    for (var i = 0; i < this._URIs.length; ++i) {
      var uri = this._URIs[i];
      var title = this._getURITitleFromHistory(uri);
      var createTxn = new PlacesCreateBookmarkTransaction(uri, -1, 
                                                          PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                          title);
      transactions.push(createTxn);
    }
    return transactions; 
  },

  /**
   * Returns a transaction for creating a new folder item representing the
   * various fields and opening arguments of the dialog.
   */
  _getCreateNewFolderTransaction:
  function BPP__getCreateNewFolderTransaction(aContainer, aIndex) {
    var annotations = [];
    var childItemsTransactions;
    if (this._URIs.length)
      childItemsTransactions = this._getTransactionsForURIList();

    if (this._description)
      annotations.push(this._getDescriptionAnnotation(this._description));

    return new PlacesCreateFolderTransaction(this._title, aContainer,
                                             aIndex, annotations,
                                             childItemsTransactions);
  },

  /**
   * Returns a transaction for creating a new live-bookmark item representing
   * the various fields and opening arguments of the dialog.
   */
  _getCreateNewLivemarkTransaction:
  function BPP__getCreateNewLivemarkTransaction(aContainer, aIndex) {
    return new PlacesCreateLivemarkTransaction(this._feedURI, this._siteURI,
                                               this._title,
                                               aContainer, aIndex);
  },

  /**
   * Dialog-accept code-path for creating a new item (any type)
   */
  _createNewItem: function BPP__getCreateItemTransaction() {
    var [container, index] = this._getInsertionPointDetails();
    var txn;

    switch (this._itemType) {
      case BOOKMARK_FOLDER:
        txn = this._getCreateNewFolderTransaction(container, index);
        break;
      case LIVEMARK_CONTAINER:
        txn = this._getCreateNewLivemarkTransaction(container, index);
        break;      
      default: // BOOKMARK_ITEM
        txn = this._getCreateNewBookmarkTransaction(container, index);
    }

    PlacesUtils.transactionManager.doTransaction(txn);
    this._itemId = PlacesUtils.bookmarks.getIdForItemAt(container, index);
  }
};
