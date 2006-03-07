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

var BookmarkPropertiesPanel = {

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

  /**
   * This method can be run on a URI parameter to ensure that it didn't
   * receive a string instead of an nsIURI object.
   */
  _assertURINotString: function PC__assertURINotString(value) {
    ASSERT((typeof(value) == "object") && !(value instanceof String),
    "This method should be passed a URI as a nsIURI object, not as a string.");
  },

  /**
   * This method should be called by the onload of the Bookmark Properties
   * dialog to initialize the state of the panel.
   *
   * @param bookmarkURI  a nsIURI object representing the bookmarked URI that
   *                     we want to view the properties of
   * @param dialogWindow the window object of the Bookmark Properties dialog
   * @param controller   a PlacesController object for interacting with the
   *                     Places system
   */
  init: function BPP_init(dialogWindow, bookmarkURI, controller) {
    this._assertURINotString(bookmarkURI);

    this._bookmarkURI = bookmarkURI;
    this._bookmarkTitle = this._bms.getItemTitle(this._bookmarkURI);
    this._dialogWindow = dialogWindow;
    this._controller = controller;

    this._initAssignableFolderResult();
    this._populateProperties();
    this._updateSize();
  },

  /**
   * This method creates a query for the set of assignable folders.
   * This only needs to be created once; when closed (using
   * root.containerOpen = false) and reopened, the results will be regenerated
   * if the data has changed since the close.
   */
  _initAssignableFolderResult: function BPP__initAssignableFolderRoot() {
    var query = this._hist.getNewQuery();
    query.setFolders([this._bms.placesRoot], 1);
    var options = this._hist.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeItems = true;

    this._assignableFolderResult = this._hist.executeQuery(query, options);
  },

  /**
   * This method fills in the data values for the fields in the dialog.
   */
  _populateProperties: function BPP__populateProperties() {
    var location = this._bookmarkURI;
    var title = this._bookmarkTitle;
    var document = this._dialogWindow.document;

    // hide standard dialog button, lest it throw off size calculation later
    this._dialogWindow.document.documentElement.getButton("accept").hidden=true;

    var nurl = document.getElementById("edit-urlbar");

    var titlebox = document.getElementById("edit-titlebox");

    nurl.value = location.spec;
    titlebox.value = title;

    var shortcutbox =
      this._dialogWindow.document.getElementById("edit-shortcutbox");
    shortcutbox.value = this._bms.getKeywordForURI(this._bookmarkURI);

    var tagArea = document.getElementById("tagbox");

    while (tagArea.hasChildNodes()) {
      tagArea.removeChild(tagArea.firstChild);
    }

    var elementDict = {};

    var root = this._assignableFolderResult.root; //Root is always a container.
    root.containerOpen = true;
    this._populateTags(root, 0, tagArea, elementDict);
    root.containerOpen = false;

    var categories = this._bms.getBookmarkFolders(location, {});

    this._updateFolderTextbox(location);

    var length = 0;
    for (key in elementDict) {
      length++;
    }

    for (var i=0; i < categories.length; i++) {
      var elm = elementDict[categories[i]];
      elm.setAttribute("selected", "true");
    }
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
  function BPP__populateTags (container, depth, parentElement, elementDict) {
    ASSERT(container.containerOpen, "The containerOpen property of the container parameter should be set to true before calling populateTags(), and then set to false again afterwards.");

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
  _createTagElement: function BPP_createTagElement(node, isSelected) {
    var tag = this._dialogWindow.document.createElement("label");
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
  tagClicked: function BPP_tagClicked(event) {
    var tagElement = event.target;

    var folderId = parseInt(tagElement.getAttribute("folderid"));

    if (tagElement.getAttribute("selected") == "true") {
      this._bms.removeItem(folderId, this._bookmarkURI);
      tagElement.setAttribute("selected", "false");
    } else {
      this._bms.insertItem(folderId, this._bookmarkURI, -1);
      tagElement.setAttribute("selected", "true");
    }

    this._updateFolderTextbox(this._bookmarkURI);
  },

  /**
   * This method sets the contents of the "Folders" textbox in the
   * Bookmark Properties panel.
   *
   * @param uri an nsIURI object representing the current bookmark's URI
   */
  _updateFolderTextbox: function BPP__updateFolderTextbox(uri) {
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
  _getFolderNameListForURI: function BPP__getFolderNameListForURI(uri) {
    var folders = this._bms.getBookmarkFolders(uri, {});
    var results = [];
    for (var i = 0; i < folders.length; i++) {
      results.push(this._bms.getFolderTitle(folders[i]));
    }
    return results.join(", ");
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
    var childDoc = this._dialogWindow.document;

    var tagbox = childDoc.getElementById("tagbox");
    tagbox.style.overflow="auto";

    var pio = childDoc.getElementById("places-info-options");
    var pig = childDoc.getElementById("places-info-grid");

    var newHeight = pio.boxObject.y + pio.boxObject.height + 5;
    this._dialogWindow.resizeTo(this._dialogWindow.innerWidth, newHeight);
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
    var titlebox = this._dialogWindow.document.getElementById("edit-titlebox");
    this._bms.setItemTitle(this._bookmarkURI, titlebox.value);

    var shortcutbox =
      this._dialogWindow.document.getElementById("edit-shortcutbox");
    this._bms.setKeywordForURI(this._bookmarkURI, shortcutbox.value);

    var urlbox = this._dialogWindow.document.getElementById("edit-urlbar");
    if (urlbox.value != this._bookmarkURI.spec) {
      /*  this._controller.changeBookmarkURI(this._bookmarkURI,
          this._uri(urlbox.value));*/
      LOG("TODO: delete existing bookmark, create new one with same folder & location.");
    }
  },

  /**
   * This method is called to exit the Bookmark Properties panel.
   */
  _hideBookmarkProperties: function BPP__hideBookmarkProperties() {
    this._dialogWindow.close();
  },
}
