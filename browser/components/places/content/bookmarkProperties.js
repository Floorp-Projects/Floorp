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

#include controller.js
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
   * The Bookmarks Service.
   */
  __bms: null,
  get _bms() {
    if (!this.__bms) {
      this.__bms =
        Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
        getService(Ci.nsINavBookmarksService);
    }
    return this.__bms;
  },

  /**
   * The Nav History Service.
   */
  __hist: null,
  get _hist() {
    if (!this.__hist) {
      this.__hist =
        Cc["@mozilla.org/browser/nav-history-service;1"].
        getService(Ci.nsINavHistoryService);
    }
    return this.__hist;
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

  _bookmarkURI: null,
  _bookmarkTitle: "",
  _dialogWindow: null,
  _parentWindow: null,
  _controller: null,
  MAX_INDENT_DEPTH: 6, // maximum indentation level of "tag" display

  EDIT_BOOKMARK_VARIANT: 0,
  ADD_BOOKMARK_VARIANT: 1,
  EDIT_HISTORY_VARIANT: 2,
  EDIT_FOLDER_VARIANT:  3,

  /**
   * The variant identifier for the current instance of the dialog.
   * The possibilities are enumerated by the constants above.
   */
  _variant: null,

  _isVariant: function BPP__isVariant(variant) {
    return this._variant == variant;
  },

  /**
   * Returns true if this variant of the dialog uses a URI as a primary
   * identifier for the item being edited.
   */

  _identifierIsURI: function BPP__identifierIsURI() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT: return false;
    default: return true;
    }
  },

  /**
   * Returns true if the URI title is editable in this dialog variant.
   */
  _isTitleEditable: function BPP__isTitleEditable() {
    switch(this._variant) {
    case this.EDIT_HISTORY_VARIANT: return false;
    default: return true;
    }
  },

  /**
   * Returns true if the URI is editable in this variant of the dialog.
   */
  _isURIEditable: function BPP__isURIEditable() {
    switch(this._variant) {
    case this.EDIT_HISTORY_VARIANT: return false;
    case this.EDIT_FOLDER_VARIANT:  return false;
    default: return true;
    }
  },

  /**
   * Returns true if the URI is visible in this variant of the dialog.
   */
  _isURIVisible: function BPP__isURIVisible() {
    switch(this._variant) {
    case this.EDIT_FOLDER_VARIANT: return false;
    default: return true;
    }
  },

  /**
   * Returns true if the the shortcut field is visible in this
   * variant of the dialog.
   */
  _isShortcutVisible: function BPP__isShortcutVisible() {
    switch(this._variant) {
    case this.EDIT_HISTORY_VARIANT: return false;
    case this.EDIT_FOLDER_VARIANT:  return false;
    default: return true;
    }
  },

  /**
   * Returns true if bookmark deletion is possible from the current
   * variant of the dialog.
   */
  _isDeletePossible: function BPP__isDeletePossible() {
    switch(this._variant) {
    case this.EDIT_HISTORY_VARIANT: return false;
    case this.ADD_BOOKMARK_VARIANT: return false;
    case this.EDIT_FOLDER_VARIANT:  return false;
    default: return true;
    }
  },

  /**
   * Returns true if the URI's folder is editable in this variant
   * of the dialog.
   */
  _isFolderEditable: function BPP__isFolderVisible() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT: return true;
    default: return false;
    }
  },

  /**
   * This method returns the correct label for the dialog's "accept"
   * button based on the variant of the dialog.
   */
  _getAcceptLabel: function BPP__getAcceptLabel() {
    switch(this._variant) {
    case this.ADD_BOOKMARK_VARIANT:
      return this._strings.getString("dialogAcceptLabelAdd");
    default: return this._strings.getString("dialogAcceptLabelEdit");
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
    case this.EDIT_HISTORY_VARIANT:
      return this._strings.getString("dialogTitleHistoryEdit");
    case this.EDIT_FOLDER_VARIANT:
      return this._strings.getString("dialogTitleFolderEdit");
    default:
      return this._strings.getString("dialogTitleBookmarkEdit");
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
   *                  UI action; or "edit" if this is being triggered from
   *                  a "properties" UI action
   *
   * @returns one of the *_VARIANT constants
   */
  _determineVariant: function BPP__determineVariant(identifier, action) {
    if (action == "add") {
      this._assertURINotString(identifier);
      if (this._bms.isBookmarked(identifier)) {
        return this.EDIT_BOOKMARK_VARIANT;
      }
      else {
        return this.ADD_BOOKMARK_VARIANT;
      }
    }
    else { /* Assume "edit" */
      if (typeof(identifier) == "number")
        return this.EDIT_FOLDER_VARIANT;

      if (this._bms.isBookmarked(identifier)) {
        return this.EDIT_BOOKMARK_VARIANT;
      }
      else {
        return this.EDIT_HISTORY_VARIANT;
      }
    }
  },

  /**
   * This method should be called by the onload of the Bookmark Properties
   * dialog to initialize the state of the panel.
   *
   * @param identifier   a nsIURI object representing the bookmarked URI or
   *                     integer folder ID of the item that
   *                     we want to view the properties of
   * @param dialogWindow the window object of the Bookmark Properties dialog
   * @param controller   a PlacesController object for interacting with the
   *                     Places system
   */
  init: function BPP_init(dialogWindow, identifier, controller, action) {
    this._variant = this._determineVariant(identifier, action);

    if (this._identifierIsURI()) {
      this._assertURINotString(identifier);
      this._bookmarkURI = identifier;
    }
    else {
      this._folderId = identifier;
    }
    this._dialogWindow = dialogWindow;
    this._controller = controller;

    this._initFolderTree();
    this._populateProperties();
    this._updateSize();
  },


  /**
   * This method initializes the folder tree.  It currently uses ._load()
   * because the tree binding doesn't allow for the setting of query options
   * to suppress non-assignable containers.
   */

  _initFolderTree: function BPP__initFolderTree() {
    this._folderTree = this._dialogWindow.document.getElementById("folderTree");
    this._folderTree.peerDropTypes = [];
    this._folderTree.childDropTypes = [];
    this._folderTree.excludeItems = true;

    var query = this._hist.getNewQuery();
    query.setFolders([this._bms.placesRoot], 1);
    var options = this._hist.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeReadOnlyFolders = true;
    options.excludeQueries = true;
    this._folderTree._load([query], options);
  },

  /**
   * This method fills in the data values for the fields in the dialog.
   */
  _populateProperties: function BPP__populateProperties() {
    var document = this._dialogWindow.document;

    if (this._identifierIsURI()) {
      this._bookmarkTitle = this._bms.getItemTitle(this._bookmarkURI);
    }
    else {
      this._bookmarkTitle = this._bms.getFolderTitle(this._folderId);
    }

    this._dialogWindow.document.title = this._getDialogTitle();

    this._dialogWindow.document.documentElement.getButton("accept").label =
      this._getAcceptLabel();

    if (!this._isDeletePossible()) {
      this._dialogWindow.document.documentElement.getButton("extra1").hidden =
        "true";
    }

    var nurl = document.getElementById("editURLBar");

    var titlebox = document.getElementById("editTitleBox");

    titlebox.value = this._bookmarkTitle;

    if (!this._isTitleEditable())
      titlebox.setAttribute("disabled", "true");

    if (this._isURIVisible()) {
      nurl.value = this._bookmarkURI.spec;

      if (!this._isURIEditable())
        nurl.setAttribute("disabled", "true");
    }
    else {
      var locationRow = document.getElementById("locationRow");
      locationRow.setAttribute("hidden", "true");
    }

    if (this._isShortcutVisible()) {
      var shortcutbox =
        this._dialogWindow.document.getElementById("editShortcutBox");
      shortcutbox.value = this._bms.getKeywordForURI(this._bookmarkURI);
    }
    else {
      var shortcutRow =
        this._dialogWindow.document.getElementById("shortcutRow");
      shortcutRow.setAttribute("hidden", "true");
    }

    if (this._isFolderEditable()) {
      this._folderTree.selectFolders([this._bms.bookmarksRoot]);
    }
    else {
      var folderRow =
        this._dialogWindow.document.getElementById("folderRow");
      folderRow.setAttribute("hidden", "true");
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
    var width = this._dialogWindow.innerWidth;
    this._dialogWindow.sizeToContent();
    this._dialogWindow.resizeTo(width, this._dialogWindow.innerHeight);
  },

 /**
  * This method implements the "Delete Bookmark" action
  * in the Bookmark Properties dialog.
  */
  dialogDeleteBookmark: function BPP_dialogDeleteBookmark() {
    this.deleteBookmark(this._bookmarkURI);
    this._hideBookmarkProperties();
  },

 /**
  * This method implements the "Done" action
  * in the Bookmark Properties dialog.
  */
  dialogDone: function BPP_dialogDone() {
    this._saveChanges();
    this._hideBookmarkProperties();
  },

  /**
   * This method deletes the bookmark corresponding to the URI stored
   * in bookmarkURI.
   */
  deleteBookmark: function BPP_deleteBookmark(bookmarkURI) {
    this._assertURINotString(bookmarkURI);

    var folders = this._bms.getBookmarkFolders(bookmarkURI, {});
    if (folders.length == 0)
      return;

    this._bms.beginUpdateBatch();
    for (var i = 0; i < folders.length; i++) {
      this._bms.removeItem(folders[i], bookmarkURI);
    }
    this._bms.endUpdateBatch();
  },

  /**
   * Save any changes that might have been made while the properties dialog
   * was open.
   */
  _saveChanges: function PBD_saveChanges() {
    var urlbox = this._dialogWindow.document.getElementById("editURLBar");
    var newURI = this._bookmarkURI;
    if (this._identifierIsURI() && this._isURIEditable())
      newURI = this._uri(urlbox.value);

    if (this._isFolderEditable()) {
      var selected =  this._folderTree.getSelectionNodes();

      for (var i = 0; i < selected.length; i++) {
        var node = selected[i];
        if (node.type == node.RESULT_TYPE_FOLDER) {
          var folder = node.QueryInterface(Ci.nsINavHistoryFolderResultNode);
          if (!folder.childrenReadOnly) {
            this._bms.insertItem(folder.folderId, newURI, -1);
          }
        }
      }
    }


    var titlebox = this._dialogWindow.document.getElementById("editTitleBox");
    if (this._identifierIsURI())
      this._bms.setItemTitle(this._bookmarkURI, titlebox.value);
    else
      this._bms.setFolderTitle(this._folderId, titlebox.value);

    if (this._isShortcutVisible()) {
      var shortcutbox =
        this._dialogWindow.document.getElementById("editShortcutBox");
      this._bms.setKeywordForURI(this._bookmarkURI, shortcutbox.value);
    }

    if (this._isVariant(this.EDIT_BOOKMARK_VARIANT) &&
        (newURI.spec != this._bookmarkURI.spec)) {
      this._controller.changeBookmarkURI(this._bookmarkURI,
                                         this._uri(urlbox.value));
    }
  },

  /**
   * This method is called to exit the Bookmark Properties panel.
   */
  _hideBookmarkProperties: function BPP__hideBookmarkProperties() {
    this._dialogWindow.close();
  }
}
