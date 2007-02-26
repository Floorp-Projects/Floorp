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
 * The Original Code is the Places Bookmark Properties.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hughes <jhughes@google.com>
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

#include ../../../../toolkit/content/debug.js

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
   * The I/O Service, useful for creating nsIURIs from strings.
   */
  __ios: null,
  get _ios() {
    if (!this.__ios) {
      this.__ios =
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    }
    return this.__ios;
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

  _bookmarkId: null,
  _bookmarkURI: null,
  _bookmarkTitle: undefined,
  _microsummaries: null,
  _dialogWindow: null,
  _parentWindow: null,
  _controller: null,

  EDIT_BOOKMARK_VARIANT: 0,
  ADD_BOOKMARK_VARIANT: 1,
  EDIT_FOLDER_VARIANT:  2,
  ADD_MULTIPLE_BOOKMARKS_VARIANT: 3,
  ADD_LIVEMARK_VARIANT: 4,
  EDIT_LIVEMARK_VARIANT: 5,

  /**
   * The variant identifier for the current instance of the dialog.
   * The possibilities are enumerated by the constants above.
   */
  _variant: null,

  /**
   * Returns true if this variant of the dialog uses a folder ID  as a primary
   * identifier for the item being edited.
   */

  _identifierIsFolderID: function BPP__identifierIsFolderID() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return true;
    default:
      return false;
    }
  },

  /**
   * Returns true if the URI is editable in this variant of the dialog.
   */
  _isURIEditable: function BPP__isURIEditable() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if the shortcut field is visible in this
   * variant of the dialog.
   */
  _isShortcutVisible: function BPP__isShortcutVisible() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if the livemark feed and site URI fields are visible.
   */

  _areLivemarkURIsVisible: function BPP__areLivemarkURIsVisible() {
    switch(this._variant) {
    case this.ADD_LIVEMARK_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return true;
    default:
      return false;
    }
  },

  /**
   * Returns true if the microsummary field is visible in this variant
   * of the dialog.
   */
  _isMicrosummaryVisible: function BPP__isMicrosummaryVisible() {
    if (!("_microsummaryVisible" in this)) {
      switch(this._variant) {
      case this.EDIT_FOLDER_VARIANT:
      case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      case this.ADD_LIVEMARK_VARIANT:
      case this.EDIT_LIVEMARK_VARIANT:
        this._microsummaryVisible = false;
        break;
      default:
        this._microsummaryVisible = true;
        break;
      }
    }
    return this._microsummaryVisible;
  },

  /**
   * Returns true if bookmark deletion is possible from the current
   * variant of the dialog.
   */
  _isDeletePossible: function BPP__isDeletePossible() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.EDIT_FOLDER_VARIANT:
      return false;
    default:
      return true;
    }
  },

  /**
   * Returns true if the URI's folder is editable in this variant
   * of the dialog.
   */
  _isFolderEditable: function BPP__isFolderEditable() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      return true;
    default:
      return false;
    }
  },

  /**
   * This method returns the correct label for the dialog's "accept"
   * button based on the variant of the dialog.
   */
  _getAcceptLabel: function BPP__getAcceptLabel() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
    case this.ADD_LIVEMARK_VARIANT:
      return this._strings.getString("dialogAcceptLabelAdd");
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      return this._strings.getString("dialogAcceptLabelAddMulti");
    default:
      return this._strings.getString("dialogAcceptLabelEdit");
    }
  },

  /**
   * This method returns the correct title for the current variant
   * of this dialog.
   */
  _getDialogTitle: function BPP__getDialogTitle() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
      return this._strings.getString("dialogTitleAdd");
    case this.EDIT_FOLDER_VARIANT:
      return this._strings.getString("dialogTitleFolderEdit");
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
      return this._strings.getString("dialogTitleAddMulti");
    case this.ADD_LIVEMARK_VARIANT:
      return this._strings.getString("dialogTitleAddLivemark");
    default:
      return this._strings.getString("dialogTitleBookmarkEdit");
    }
  },

  /**
   * Returns a string representing the folder tree selection type for
   * the given dialog variant.  This is either "single" when you can only
   * select one folder (usually because we're dealing with the location
   * of a child folder, which can only have one parent), or "multiple"
   * when you can select multiple folders (bookmarks can be in multiple
   * folders).
   */
  _getFolderSelectionType: function BPP__getFolderSelectionType() {
    switch(this._variant) {
    case this.ADD_MULTIPLE_BOOKMARKS_VARIANT:
    case this.EDIT_FOLDER_VARIANT:
    case this.EDIT_LIVEMARK_VARIANT:
      return "single";
    default:
      return "multiple";
    }
  },

  /**
   * This method can be run on a URI parameter to ensure that it didn't
   * receive a string instead of an nsIURI object.
   */
  _assertURINotString: function BPP__assertURINotString(value) {
    NS_ASSERT((typeof(value) == "object") && !(value instanceof String),
    "This method should be passed a URI as a nsIURI object, not as a string.");
  },

  /**
   * Determines the correct variant of the dialog to display depending
   * on which action is passed in and the properties of the identifier value
   * (generally either a URI or a folder ID).
   *
   * NOTE: It's currently not possible to create the dialog with a folder
   *       id and "add" mode.
   *
   * @param identifier the URI or folder ID to display the properties for
   * @param action -- "add" if this is being triggered from an "add bookmark"
   *                  UI action; or "editfolder" or "edititem" if this is being
   *                  triggered from a "properties" UI action; or "addmulti" if
   *                  we're trying to create multiple bookmarks.
   *
   * @returns one of the *_VARIANT constants
   */
  _determineVariant: function BPP__determineVariant(identifier, action) {
    if (action == "add") {
      this._assertURINotString(identifier);
      return this.ADD_BOOKMARK_VARIANT;
    }
    else if (action == "addmulti") {
      return this.ADD_MULTIPLE_BOOKMARKS_VARIANT;
    }
    else if (typeof(identifier) == "number") {
      if (action == "edititem") {
        return this.EDIT_BOOKMARK_VARIANT;
      }
      else if (action == "editfolder") {
        if (PlacesUtils.livemarks.isLivemark(identifier))
          return this.EDIT_LIVEMARK_VARIANT;
        return this.EDIT_FOLDER_VARIANT;
      }
    }
  },

  /**
   * This method returns the title string corresponding to a given URI.
   * If none is available from the bookmark service (probably because
   * the given URI doesn't appear in bookmarks or history), we synthesize
   * a title from the first 100 characters of the URI.
   *
   * @param uri  a nsIURI object for which we want the title
   *
   * @returns a title string
   */

  _getURITitleFromHistory: function BPP__getURITitleFromHistory(uri) {
    this._assertURINotString(uri);

    // get the title from History
    return PlacesUtils.history.getPageTitle(uri);
  },

  /**
   * This method should be called by the onload of the Bookmark Properties
   * dialog to initialize the state of the panel.
   *
   * @param dialogWindow the window object of the Bookmark Properties dialog
   * @param tm           the transaction Manager of the opener.
   * @param action       the desired user action; see determineVariant()
   * @param identifier   a nsIURI object representing the bookmarked URI or
   *                     integer folder ID of the item that
   *                     we want to view the properties of
   * @param title        a string representing the desired title of the
   *                     bookmark; undefined means "pick a default title"
   */
  init: function BPP_init(dialogWindow, tm, action, identifier, title) {
    this._variant = this._determineVariant(identifier, action);

    if (this._variant == this.EDIT_BOOKMARK_VARIANT) {
      this._bookmarkId = identifier;
      this._bookmarkURI = PlacesUtils.bookmarks.getBookmarkURI(this._bookmarkId);
      this._folderId = PlacesUtils.bookmarks.getFolderIdForItem(identifier);
    }
    else if (this._variant == this.ADD_BOOKMARK_VARIANT) {
      this._assertURINotString(identifier);
      this._bookmarkURI = identifier;
    }
    else if (this._identifierIsFolderID()) {
      this._folderId = identifier;
    }
    else if (this._variant == this.ADD_MULTIPLE_BOOKMARKS_VARIANT) {
      this._URIList = identifier;
    }

    this._bookmarkTitle = title;
    this._dialogWindow = dialogWindow;
    this._tm = tm;

    this._initFolderTree();
    this._populateProperties();
    this._updateSize();
  },


  /**
   * This method initializes the folder tree. 
   */

  _initFolderTree: function BPP__initFolderTree() {
    this._folderTree = this._dialogWindow.document.getElementById("folderTree");
    this._folderTree.peerDropTypes = [];
    this._folderTree.childDropTypes = [];
    this._folderTree.excludeItems = true;
    this._folderTree.setAttribute("seltype", this._getFolderSelectionType());

    var query = PlacesUtils.history.getNewQuery();
    query.setFolders([PlacesUtils.bookmarks.placesRoot], 1);
    var options = PlacesUtils.history.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeReadOnlyFolders = true;
    options.excludeQueries = true;
    this._folderTree.load([query], options);
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
      this._microsummaryVisible = false;
      this._hide("microsummaryRow");
      return;
    }
    this._microsummaries.addObserver(this._microsummaryObserver);
    this._rebuildMicrosummaryPicker();
  },

  /**
   * This is a shorter form of getElementById for the dialog document.
   * Given a XUL element ID from the dialog, returns the corresponding
   * DOM element.
   *
   * @param  XUL element ID
   * @returns corresponding DOM element, or null if none found
   */

  _element: function BPP__element(id) {
    return this._dialogWindow.document.getElementById(id);
  },

  /**
   * Hides the XUL element with the given ID.
   *
   * @param  string ID of the XUL element to hide
   */

  _hide: function BPP__hide(id) {
    this._element(id).setAttribute("hidden", "true");
  },

  /**
   * This method fills in the data values for the fields in the dialog.
   */
  _populateProperties: function BPP__populateProperties() {
    var document = this._dialogWindow.document;

    /* The explicit comparison against undefined here allows creators to pass
     * "" to init() if they wish to have no title. */
    if (this._bookmarkTitle === undefined) {
      if (this._variant == this.EDIT_BOOKMARK_VARIANT) {
        this._bookmarkTitle = PlacesUtils.bookmarks.getItemTitle(this._bookmarkId);
      }
      else if (this._variant == this.ADD_BOOKMARK_VARIANT) {
        this._bookmarkTitle = this._getURITitleFromHistory(this._bookmarkURI);
      }
      else if (this._identifierIsFolderID()) {
        this._bookmarkTitle = PlacesUtils.bookmarks.getFolderTitle(this._folderId);
      }
      else if (this._variant == this.ADD_MULTIPLE_BOOKMARKS_VARIANT) {
        this._bookmarkTitle = this._strings.getString("bookmarkAllTabsDefault");
      }
    }

    this._dialogWindow.document.title = this._getDialogTitle();

    this._dialogWindow.document.documentElement.getButton("accept").label =
      this._getAcceptLabel();

    if (!this._isDeletePossible()) {
      this._dialogWindow.document.documentElement.getButton("extra1").hidden =
        "true";
    }

    var nurl = this._element("editURLBar");

    var titlebox = this._element("editTitleBox");

    titlebox.value = this._bookmarkTitle;

    if (this._isURIEditable()) {
      nurl.value = this._bookmarkURI.spec;
    }
    else {
      this._hide("locationRow");
    }

    if (this._areLivemarkURIsVisible()) {
      if (this._identifierIsFolderID()) {
        var feedURI = PlacesUtils.livemarks.getFeedURI(this._folderId);
        if (feedURI)
          this._element("editLivemarkFeedLocationBox").value = feedURI.spec;
        var siteURI = PlacesUtils.livemarks.getSiteURI(this._folderId);
        if (siteURI)
          this._element("editLivemarkSiteLocationBox").value = siteURI.spec;
      }
    } else {
      this._hide("livemarkFeedLocationRow");
      this._hide("livemarkSiteLocationRow");
    }

    if (this._isShortcutVisible()) {
      var shortcutbox = this._element("editShortcutBox");
      shortcutbox.value = PlacesUtils.bookmarks.getKeywordForBookmark(this._bookmarkId);
    }
    else {
      this._hide("shortcutRow");
    }

    if (this._isMicrosummaryVisible()) {
      this._initMicrosummaryPicker();
    }
    else {
      this._hide("microsummaryRow");
    }

    if (this._isFolderEditable()) {
      this._folderTree.selectFolders([PlacesUtils.bookmarks.bookmarksRoot]);
    }
    else {
      this._hide("folderRow");
    }
  },

  //XXXDietrich - bug 370215 - update to use bookmark id once 360133 is fixed.
  _rebuildMicrosummaryPicker: function BPP__rebuildMicrosummaryPicker() {
    var microsummaryMenuList = document.getElementById("microsummaryMenuList");
    var microsummaryMenuPopup = document.getElementById("microsummaryMenuPopup");

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
      if (microsummary.content != null)
        menuItem.setAttribute("label", microsummary.content);
      else {
        menuItem.setAttribute("label", microsummary.generator ? microsummary.generator.name
                                                               : microsummary.generatorURI.spec);
        microsummary.update();
      }

      microsummaryMenuPopup.appendChild(menuItem);

      // Select the item if this is the current microsummary for the bookmark.
      if (this._mss.isMicrosummary(this._bookmarkURI, microsummary))
        microsummaryMenuList.selectedItem = menuItem;
    }
  },

  _microsummaryObserver: {
    interfaces: [Ci.nsIMicrosummaryObserver, Ci.nsISupports],
  
    QueryInterface: function (iid) {
      //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      if (!iid.equals(Ci.nsIMicrosummaryObserver) &&
          !iid.equals(Ci.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    },
  
    onContentLoaded: function(microsummary) {
      BookmarkPropertiesPanel._rebuildMicrosummaryPicker();
    },

    onElementAppended: function(microsummary) {
      BookmarkPropertiesPanel._rebuildMicrosummaryPicker();
    }
  },
  
  /**
   * Makes a URI from a spec.
   * @param   spec
   *          The string spec of the URI
   * @returns A URI object for the spec. 
   */
  _uri: function PC__uri(spec) {
    return this._ios.newURI(spec, null, null);
  },

  /**
   * Size the dialog to fit its contents.
   */
  _updateSize: function BPP__updateSize() {
    var width = this._dialogWindow.outerWidth;
    this._dialogWindow.sizeToContent();
    this._dialogWindow.resizeTo(width, this._dialogWindow.outerHeight);
  },

 /**
  * This method implements the "Delete Bookmark" action
  * in the Bookmark Properties dialog.
  */
  dialogDeleteBookmark: function BPP_dialogDeleteBookmark() {
    this.deleteItem();
    this._hideBookmarkProperties();
  },

 /**
  * This method implements the "Done" action
  * in the Bookmark Properties dialog.
  */
  dialogDone: function BPP_dialogDone() {
    if (this._isMicrosummaryVisible() && this._microsummaries)
      this._microsummaries.removeObserver(this._microsummaryObserver);
    this._saveChanges();
    this._hideBookmarkProperties();
  },
  
  dialogCancel: function BPP_dialogCancel() {
    if (this._isMicrosummaryVisible() && this._microsummaries)
      this._microsummaries.removeObserver(this._microsummaryObserver);
    this._hideBookmarkProperties();
  },

  /**
   * This method deletes the currently loaded item in this dialog.
   */
  deleteItem: function BPP_deleteItem() {
    var transactions = [];

    if (this._variant == this.EDIT_BOOKMARK_VARIANT) {
      var index = PlacesUtils.bookmarks.getItemIndex(this._bookmarkId);
      var transaction = new PlacesRemoveItemTransaction(
        this._bookmarkId, this._bookmarkURI, this._folderId, index);
      transactions.push(transaction);
    }
    else { // This is a folder Id
      var transaction = new PlacesRemoveFolderTransaction(this._folderId);
      transactions.push(transaction);
    }

    var aggregate =
      new PlacesAggregateTransaction(this._getDialogTitle(), transactions);
    this._tm.doTransaction(aggregate);
  },

  /**
   * This method checks the current state of the input fields in the
   * dialog, and if any of them are in an invalid state, it will disable
   * the submit button.  This method should be called after every
   * significant change to the input.
   */
  validateChanges: function BPP_validateChanges() {
    this._dialogWindow.document.documentElement.getButton("accept").disabled =
      !this._inputIsValid();
  },

  /**
   * This method checks to see if the input fields are in a valid state.
   *
   * @returns  true if the input is valid, false otherwise
   */
  _inputIsValid: function BPP__inputIsValid() {
    // When in multiple select mode, it's possible to deselect all rows,
    // but you have to file your bookmark in at least one folder.
    if (this._isFolderEditable()) {
      if (this._folderTree.getSelectionNodes().length == 0)
        return false;
    }

    if (this._isURIEditable() && !this._containsValidURI("editURLBar"))
      return false;

    // Feed Location has to be a valid URI;
    // Site Location has to be a valid URI or empty
    if (this._areLivemarkURIsVisible()) {
      if (!this._containsValidURI("editLivemarkFeedLocationBox"))
        return false;
      if (!this._containsValidURI("editLivemarkSiteLocationBox") &&
          (this._element("editLivemarkSiteLocationBox").value.length > 0))
        return false;
    }

    return true;
  },

  /**
   * Determines whether the XUL textbox with the given ID contains a
   * string that can be converted into an nsIURI.
   *
   * @param textboxID the ID of the textbox element whose contents we'll test
   *
   * @returns true if the textbox contains a valid URI string, false otherwise
   */
  _containsValidURI: function BPP__containsValidURI(textboxID) {
    try {
      var uri = this._uri(this._element(textboxID).value);
    } catch (e) {
      return false;
    }
    return true;
  },

  /**
   * Save any changes that might have been made while the properties dialog
   * was open.
   */
  _saveChanges: function BPP__saveChanges() {
    var transactions = [];
    var urlbox = this._element("editURLBar");
    var titlebox = this._element("editTitleBox");
    var newURI = this._bookmarkURI;
    if (this._variant == this.ADD_BOOKMARK_VARIANT || this._isURIEditable())
      newURI = this._uri(urlbox.value);

    // adding one or more bookmarks
    if (this._isFolderEditable()) {
      var folder = PlacesUtils.bookmarks.bookmarksRoot;
      var selected =  this._folderTree.getSelectionNodes();

      // add single bookmark
      if (this._variant == this.ADD_BOOKMARK_VARIANT) {
        // get folder id
        if (selected.length > 0) {
          var node = selected[0];
          if (node.type == node.RESULT_TYPE_FOLDER) {
            var folderNode = node.QueryInterface(Ci.nsINavHistoryFolderResultNode);
            if (!folderNode.childrenReadOnly)
              folder = folderNode.folderId;
          }
        }
        var txnCreateItem = new PlacesCreateItemTransaction(newURI, folder, -1);
        txnCreateItem.childTransactions.push(
          new PlacesEditItemTitleTransaction(-1, titlebox.value));
        transactions.push(txnCreateItem);
      }
      // bookmark multiple URIs
      else if (this._variant == this.ADD_MULTIPLE_BOOKMARKS_VARIANT) {
        var node = selected[0];
        var folder = node.QueryInterface(Ci.nsINavHistoryFolderResultNode);

        var newFolderTrans = new PlacesCreateFolderTransaction(
            titlebox.value, folder.folderId, -1);

        for (var i = 0; i < this._URIList.length; ++i) {
          var uri = this._URIList[i];
          var txn = new PlacesCreateItemTransaction(uri, -1, -1);
          txn.childTransactions.push(
            new PlacesEditItemTitleTransaction(uri, this._getURITitleFromHistory(uri)));
          newFolderTrans.childTransactions.push(txn);
        }

        transactions.push(newFolderTrans);
      }
    }

    // editing a bookmark
    if (this._variant == this.EDIT_BOOKMARK_VARIANT) {
      transactions.push(
        new PlacesEditItemTitleTransaction(this._bookmarkId, titlebox.value));
    }
    // editing a folder or livemark
    else if (this._identifierIsFolderID()) {
      if (this._areLivemarkURIsVisible()) {
        if (this._identifierIsFolderID()) {
          var feedURIString =
            this._element("editLivemarkFeedLocationBox").value;
          var feedURI = this._uri(feedURIString);
          transactions.push(
            new PlacesEditLivemarkFeedURITransaction(this._folderId, feedURI));

          // Site Location is empty, we can set its URI to null
          var siteURI = null;
          var siteURIString =
            this._element("editLivemarkSiteLocationBox").value;
          if (siteURIString.length > 0)
            siteURI = this._uri(siteURIString);
          transactions.push(
            new PlacesEditLivemarkSiteURITransaction(this._folderId, siteURI));
        }
      }

      transactions.push(
        new PlacesEditFolderTitleTransaction(this._folderId, titlebox.value));
    }

    // keyword
    if (this._isShortcutVisible()) {
      var shortcutbox =
        this._element("editShortcutBox");
      if (shortcutbox.value.length > 0)
        transactions.push(
          new PlacesEditBookmarkKeywordTransaction(this._bookmarkId,
                                                   shortcutbox.value));
    }

    // change bookmark URI
    if (this._variant == this.EDIT_BOOKMARK_VARIANT &&
        (newURI.spec != this._bookmarkURI.spec)) {
      // XXXDietrich - needs to be transactionalized
      PlacesUtils.changeBookmarkURI(this._bookmarkId, newURI);
    }

    // microsummaries
    if (this._isMicrosummaryVisible()) {
      var menuList = document.getElementById("microsummaryMenuList");

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
      // XXXDietrich - bug 370215 - update to use bookmark id once 360133 is fixed.
      if ((newMicrosummary == null &&
           this._mss.hasMicrosummary(this._bookmarkURI)) ||
          (newMicrosummary != null &&
           !this._mss.isMicrosummary(this._bookmarkURI, newMicrosummary))) {
        transactions.push(
          new PlacesEditBookmarkMicrosummaryTransaction(this._bookmarkURI,
                                                        newMicrosummary));
      }
    }

    // If we have any changes to perform, do them via the
    // transaction manager in the PlacesController so they can be undone.
    if (transactions.length > 0) {
      var aggregate =
        new PlacesAggregateTransaction(this._getDialogTitle(), transactions);
      this._tm.doTransaction(aggregate);
    }
  },

  /**
   * This method is called to exit the Bookmark Properties panel.
   */
  _hideBookmarkProperties: function BPP__hideBookmarkProperties() {
    this._dialogWindow.close();
  }
};
