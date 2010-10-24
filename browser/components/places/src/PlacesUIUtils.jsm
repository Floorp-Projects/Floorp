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
 * The Original Code is the Places Command Controller.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
 *   Myk Melez <myk@mozilla.org>
 *   Asaf Romano <mano@mozilla.com>
 *   Sungjoon Steve Won <stevewon@gmail.com>
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Marco Bonardo <mak77@bonardo.net>
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

var EXPORTED_SYMBOLS = ["PlacesUIUtils"];

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "PlacesUtils", function() {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils;
});

var PlacesUIUtils = {
  ORGANIZER_LEFTPANE_VERSION: 6,
  ORGANIZER_FOLDER_ANNO: "PlacesOrganizer/OrganizerFolder",
  ORGANIZER_QUERY_ANNO: "PlacesOrganizer/OrganizerQuery",

  LOAD_IN_SIDEBAR_ANNO: "bookmarkProperties/loadInSidebar",
  DESCRIPTION_ANNO: "bookmarkProperties/description",

  TYPE_TAB_DROP: "application/x-moz-tabbrowser-tab",

  /**
   * Makes a URI from a spec, and do fixup
   * @param   aSpec
   *          The string spec of the URI
   * @returns A URI object for the spec.
   */
  createFixedURI: function PUIU_createFixedURI(aSpec) {
    return URIFixup.createFixupURI(aSpec, Ci.nsIURIFixup.FIXUP_FLAG_NONE);
  },

  /**
   * Wraps a string in a nsISupportsString wrapper
   * @param   aString
   *          The string to wrap
   * @returns A nsISupportsString object containing a string.
   */
  _wrapString: function PUIU__wrapString(aString) {
    var s = Cc["@mozilla.org/supports-string;1"].
            createInstance(Ci.nsISupportsString);
    s.data = aString;
    return s;
  },

  getFormattedString: function PUIU_getFormattedString(key, params) {
    return bundle.formatStringFromName(key, params, params.length);
  },

  getString: function PUIU_getString(key) {
    return bundle.GetStringFromName(key);
  },

  /**
   * Get a transaction for copying a uri item from one container to another
   * as a bookmark.
   * @param   aData
   *          JSON object of dropped or pasted item properties
   * @param   aContainer
   *          The container being copied into
   * @param   aIndex
   *          The index within the container the item is copied to
   * @returns A nsITransaction object that performs the copy.
   */
  _getURIItemCopyTransaction: function (aData, aContainer, aIndex) {
    return this.ptm.createItem(PlacesUtils._uri(aData.uri), aContainer, aIndex,
                               aData.title, "");
  },

  /**
   * Get a transaction for copying a bookmark item from one container to
   * another.
   * @param   aData
   *          JSON object of dropped or pasted item properties
   * @param   aContainer
   *          The container being copied into
   * @param   aIndex
   *          The index within the container the item is copied to
   * @param   [optional] aExcludeAnnotations
   *          Optional, array of annotations (listed by their names) to exclude
   *          when copying the item.
   * @returns A nsITransaction object that performs the copy.
   */
  _getBookmarkItemCopyTransaction:
  function PUIU__getBookmarkItemCopyTransaction(aData, aContainer, aIndex,
                                                aExcludeAnnotations) {
    var itemURL = PlacesUtils._uri(aData.uri);
    var itemTitle = aData.title;
    var keyword = aData.keyword || null;
    var annos = aData.annos || [];
    // always exclude GUID when copying any item
    var excludeAnnos = [PlacesUtils.GUID_ANNO];
    if (aExcludeAnnotations)
      excludeAnnos = excludeAnnos.concat(aExcludeAnnotations);
    annos = annos.filter(function(aValue, aIndex, aArray) {
      return excludeAnnos.indexOf(aValue.name) == -1;
    });
    var childTxns = [];
    if (aData.dateAdded)
      childTxns.push(this.ptm.editItemDateAdded(null, aData.dateAdded));
    if (aData.lastModified)
      childTxns.push(this.ptm.editItemLastModified(null, aData.lastModified));
    if (aData.tags) {
      var tags = aData.tags.split(", ");
      // filter out tags already present, so that undo doesn't remove them
      // from pre-existing bookmarks
      var storedTags = PlacesUtils.tagging.getTagsForURI(itemURL);
      tags = tags.filter(function (aTag) {
        return (storedTags.indexOf(aTag) == -1);
      }, this);
      if (tags.length)
        childTxns.push(this.ptm.tagURI(itemURL, tags));
    }

    return this.ptm.createItem(itemURL, aContainer, aIndex, itemTitle, keyword,
                               annos, childTxns);
  },

  /**
   * Gets a transaction for copying (recursively nesting to include children)
   * a folder (or container) and its contents from one folder to another.
   *
   * @param   aData
   *          Unwrapped dropped folder data - Obj containing folder and children
   * @param   aContainer
   *          The container we are copying into
   * @param   aIndex
   *          The index in the destination container to insert the new items
   * @returns A nsITransaction object that will perform the copy.
   */
  _getFolderCopyTransaction:
  function PUIU__getFolderCopyTransaction(aData, aContainer, aIndex) {
    function getChildItemsTransactions(aChildren) {
      var childItemsTransactions = [];
      var cc = aChildren.length;
      var index = aIndex;
      for (var i = 0; i < cc; ++i) {
        var txn = null;
        var node = aChildren[i];

        // Make sure that items are given the correct index, this will be
        // passed by the transaction manager to the backend for the insertion.
        // Insertion behaves differently if index == DEFAULT_INDEX (append)
        if (aIndex != PlacesUtils.bookmarks.DEFAULT_INDEX)
          index = i;

        if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
          if (node.livemark && node.annos) // node is a livemark
            txn = PlacesUIUtils._getLivemarkCopyTransaction(node, aContainer, index);
          else
            txn = PlacesUIUtils._getFolderCopyTransaction(node, aContainer, index);
        }
        else if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR)
          txn = PlacesUIUtils.ptm.createSeparator(-1, index);
        else if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE)
          txn = PlacesUIUtils._getBookmarkItemCopyTransaction(node, -1, index);

        if (txn)
          childItemsTransactions.push(txn);
        else
          throw("Unexpected item under a bookmarks folder");
      }
      return childItemsTransactions;
    }

    // tag folders use tag transactions
    if (aContainer == PlacesUtils.tagsFolderId) {
      var txns = [];
      if (aData.children) {
        aData.children.forEach(function(aChild) {
          txns.push(this.ptm.tagURI(PlacesUtils._uri(aChild.uri), [aData.title]));
        }, this);
      }
      return this.ptm.aggregateTransactions("addTags", txns);
    }
    else if (aData.livemark && aData.annos) {
      // Place is a Livemark Container
      return this._getLivemarkCopyTransaction(aData, aContainer, aIndex);
    }
    else {
      var childItems = getChildItemsTransactions(aData.children);
      if (aData.dateAdded)
        childItems.push(this.ptm.editItemDateAdded(null, aData.dateAdded));
      if (aData.lastModified)
        childItems.push(this.ptm.editItemLastModified(null, aData.lastModified));

      var annos = aData.annos || [];
      annos = annos.filter(function(aAnno) {
        // always exclude GUID when copying any item
        return aAnno.name != PlacesUtils.GUID_ANNO;
      });
      return this.ptm.createFolder(aData.title, aContainer, aIndex, annos, childItems);
    }
  },

  _getLivemarkCopyTransaction:
  function PUIU__getLivemarkCopyTransaction(aData, aContainer, aIndex) {
    if (!aData.livemark || !aData.annos)
      throw("node is not a livemark");
    // Place is a Livemark Container
    var feedURI = null;
    var siteURI = null;
    aData.annos = aData.annos.filter(function(aAnno) {
      if (aAnno.name == PlacesUtils.LMANNO_FEEDURI) {
        feedURI = PlacesUtils._uri(aAnno.value);
        return false;
      }
      else if (aAnno.name == PlacesUtils.LMANNO_SITEURI) {
        siteURI = PlacesUtils._uri(aAnno.value);
        return false;
      }
      // always exclude GUID when copying any item
      return aAnno.name != PlacesUtils.GUID_ANNO;
    });
    return this.ptm.createLivemark(feedURI, siteURI, aData.title, aContainer,
                                   aIndex, aData.annos);
  },

  /**
   * Constructs a Transaction for the drop or paste of a blob of data into
   * a container.
   * @param   data
   *          The unwrapped data blob of dropped or pasted data.
   * @param   type
   *          The content type of the data
   * @param   container
   *          The container the data was dropped or pasted into
   * @param   index
   *          The index within the container the item was dropped or pasted at
   * @param   copy
   *          The drag action was copy, so don't move folders or links.
   * @returns An object implementing nsITransaction that can perform
   *          the move/insert.
   */
  makeTransaction: function PUIU_makeTransaction(data, type, container,
                                                 index, copy) {
    switch (data.type) {
      case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
        if (copy)
          return this._getFolderCopyTransaction(data, container, index);
        // Otherwise move the item.
        return this.ptm.moveItem(data.id, container, index);
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE:
        if (data.id == -1) // Not bookmarked.
          return this._getURIItemCopyTransaction(data, container, index);

        if (copy)
          return this._getBookmarkItemCopyTransaction(data, container, index);
        // Otherwise move the item.
        return this.ptm.moveItem(data.id, container, index);
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
        // There is no data in a separator, so copying it just amounts to
        // inserting a new separator.
        if (copy)
          return this.ptm.createSeparator(container, index);
        // Otherwise move the item.
        return this.ptm.moveItem(data.id, container, index);
        break;
      default:
        if (type == PlacesUtils.TYPE_X_MOZ_URL ||
            type == PlacesUtils.TYPE_UNICODE ||
            type == this.TYPE_TAB_DROP) {
          var title = (type != PlacesUtils.TYPE_UNICODE) ? data.title :
                                                             data.uri;
          return this.ptm.createItem(PlacesUtils._uri(data.uri),
                                     container, index, title);
        }
    }
    return null;
  },

  /**
   * Methods to show the bookmarkProperties dialog in its various modes.
   *
   * The showMinimalAdd* methods open the dialog by its alternative URI. Thus
   * they persist the dialog dimensions separately from the showAdd* methods.
   * Note these variants also do not return the dialog "performed" state since
   * they may not open the dialog modally.
   */

  /**
   * Shows the "Add Bookmark" dialog.
   *
   * @param [optional] aURI
   *        An nsIURI object for which the "add bookmark" dialog is
   *        to be shown.
   * @param [optional] aTitle
   *        The default title for the new bookmark.
   * @param [optional] aDescription
            The default description for the new bookmark
   * @param [optional] aDefaultInsertionPoint
   *        The default insertion point for the new item. If set, the folder
   *        picker would be hidden unless aShowPicker is set to true, in which
   *        case the dialog only uses the folder identifier from the insertion
   *        point as the initially selected item in the folder picker.
   * @param [optional] aShowPicker
   *        see above
   * @param [optional] aLoadInSidebar
   *        If true, the dialog will default to load the new item in the
   *        sidebar (as a web panel).
   * @param [optional] aKeyword
   *        The default keyword for the new bookmark. The keyword field
   *        will be shown in the dialog if this is used.
   * @param [optional] aPostData
   *        POST data for POST-style keywords.
   * @param [optional] aCharSet
   *        The character set for the bookmarked page.
   * @return true if any transaction has been performed.
   *
   * Notes:
   *  - the location, description and "loadInSidebar" fields are
   *    visible only if there is no initial URI (aURI is null).
   *  - When aDefaultInsertionPoint is not set, the dialog defaults to the
   *    bookmarks root folder.
   */
  showAddBookmarkUI: function PUIU_showAddBookmarkUI(aURI,
                                                     aTitle,
                                                     aDescription,
                                                     aDefaultInsertionPoint,
                                                     aShowPicker,
                                                     aLoadInSidebar,
                                                     aKeyword,
                                                     aPostData,
                                                     aCharSet) {
    var info = {
      action: "add",
      type: "bookmark"
    };

    if (aURI)
      info.uri = aURI;

    // allow default empty title
    if (typeof(aTitle) == "string")
      info.title = aTitle;

    if (aDescription)
      info.description = aDescription;

    if (aDefaultInsertionPoint) {
      info.defaultInsertionPoint = aDefaultInsertionPoint;
      if (!aShowPicker)
        info.hiddenRows = ["folderPicker"];
    }

    if (aLoadInSidebar)
      info.loadBookmarkInSidebar = true;

    if (typeof(aKeyword) == "string") {
      info.keyword = aKeyword;
      if (typeof(aPostData) == "string")
        info.postData = aPostData;
      if (typeof(aCharSet) == "string")
        info.charSet = aCharSet;
    }

    return this._showBookmarkDialog(info);
  },

  /**
   * @see showAddBookmarkUI
   * This opens the dialog with only the name and folder pickers visible by
   * default.
   *
   * You can still pass in the various paramaters as the default properties
   * for the new bookmark.
   *
   * The keyword field will be visible only if the aKeyword parameter
   * was used.
   */
  showMinimalAddBookmarkUI:
  function PUIU_showMinimalAddBookmarkUI(aURI, aTitle, aDescription,
                                         aDefaultInsertionPoint, aShowPicker,
                                         aLoadInSidebar, aKeyword, aPostData,
                                         aCharSet) {
    var info = {
      action: "add",
      type: "bookmark",
      hiddenRows: ["description"]
    };
    if (aURI)
      info.uri = aURI;

    // allow default empty title
    if (typeof(aTitle) == "string")
      info.title = aTitle;

    if (aDescription)
      info.description = aDescription;

    if (aDefaultInsertionPoint) {
      info.defaultInsertionPoint = aDefaultInsertionPoint;
      if (!aShowPicker)
        info.hiddenRows.push("folderPicker");
    }

    if (aLoadInSidebar)
      info.loadBookmarkInSidebar = true;
    else
      info.hiddenRows = info.hiddenRows.concat(["location", "loadInSidebar"]);

    if (typeof(aKeyword) == "string") {
      info.keyword = aKeyword;
      // Hide the Tags field if we are adding a keyword.
      info.hiddenRows.push("tags");
      // Keyword related params.
      if (typeof(aPostData) == "string")
        info.postData = aPostData;
      if (typeof(aCharSet) == "string")
        info.charSet = aCharSet;
    }
    else
      info.hiddenRows.push("keyword");

    return this._showBookmarkDialog(info, true);
  },

  /**
   * Shows the "Add Live Bookmark" dialog.
   *
   * @param [optional] aFeedURI
   *        The feed URI for which the dialog is to be shown (nsIURI).
   * @param [optional] aSiteURI
   *        The site URI for the new live-bookmark (nsIURI).
   * @param [optional] aDefaultInsertionPoint
   *        The default insertion point for the new item. If set, the folder
   *        picker would be hidden unless aShowPicker is set to true, in which
   *        case the dialog only uses the folder identifier from the insertion
   *        point as the initially selected item in the folder picker.
   * @param [optional] aShowPicker
   *        see above
   * @return true if any transaction has been performed.
   *
   * Notes:
   *  - the feedURI and description fields are visible only if there is no
   *    initial feed URI (aFeedURI is null).
   *  - When aDefaultInsertionPoint is not set, the dialog defaults to the
   *    bookmarks root folder.
   */
  showAddLivemarkUI: function PUIU_showAddLivemarkURI(aFeedURI,
                                                      aSiteURI,
                                                      aTitle,
                                                      aDescription,
                                                      aDefaultInsertionPoint,
                                                      aShowPicker) {
    var info = {
      action: "add",
      type: "livemark"
    };

    if (aFeedURI)
      info.feedURI = aFeedURI;
    if (aSiteURI)
      info.siteURI = aSiteURI;

    // allow default empty title
    if (typeof(aTitle) == "string")
      info.title = aTitle;

    if (aDescription)
      info.description = aDescription;

    if (aDefaultInsertionPoint) {
      info.defaultInsertionPoint = aDefaultInsertionPoint;
      if (!aShowPicker)
        info.hiddenRows = ["folderPicker"];
    }
    return this._showBookmarkDialog(info);
  },

  /**
   * @see showAddLivemarkUI
   * This opens the dialog with only the name and folder pickers visible by
   * default.
   *
   * You can still pass in the various paramaters as the default properties
   * for the new live-bookmark.
   */
  showMinimalAddLivemarkUI:
  function PUIU_showMinimalAddLivemarkURI(aFeedURI, aSiteURI, aTitle,
                                          aDescription, aDefaultInsertionPoint,
                                          aShowPicker) {
    var info = {
      action: "add",
      type: "livemark",
      hiddenRows: ["feedLocation", "siteLocation", "description"]
    };

    if (aFeedURI)
      info.feedURI = aFeedURI;
    if (aSiteURI)
      info.siteURI = aSiteURI;

    // allow default empty title
    if (typeof(aTitle) == "string")
      info.title = aTitle;

    if (aDescription)
      info.description = aDescription;

    if (aDefaultInsertionPoint) {
      info.defaultInsertionPoint = aDefaultInsertionPoint;
      if (!aShowPicker)
        info.hiddenRows.push("folderPicker");
    }
    return this._showBookmarkDialog(info, true);
  },

  /**
   * Show an "Add Bookmarks" dialog to allow the adding of a folder full
   * of bookmarks corresponding to the objects in the uriList.  This will
   * be called most often as the result of a "Bookmark All Tabs..." command.
   *
   * @param aURIList  List of nsIURI objects representing the locations
   *                  to be bookmarked.
   * @return true if any transaction has been performed.
   */
  showMinimalAddMultiBookmarkUI: function PUIU_showAddMultiBookmarkUI(aURIList) {
    if (aURIList.length == 0)
      throw("showAddMultiBookmarkUI expects a list of nsIURI objects");
    var info = {
      action: "add",
      type: "folder",
      hiddenRows: ["description"],
      URIList: aURIList
    };
    return this._showBookmarkDialog(info, true);
  },

  /**
   * Opens the properties dialog for a given item identifier.
   *
   * @param aItemId
   *        item identifier for which the properties are to be shown
   * @param aType
   *        item type, either "bookmark" or "folder"
   * @param [optional] aReadOnly
   *        states if properties dialog should be readonly
   * @return true if any transaction has been performed.
   */
  showItemProperties: function PUIU_showItemProperties(aItemId, aType, aReadOnly) {
    var info = {
      action: "edit",
      type: aType,
      itemId: aItemId,
      readOnly: aReadOnly
    };
    return this._showBookmarkDialog(info);
  },

  /**
   * Shows the "New Folder" dialog.
   *
   * @param [optional] aTitle
   *        The default title for the new bookmark.
   * @param [optional] aDefaultInsertionPoint
   *        The default insertion point for the new item. If set, the folder
   *        picker would be hidden unless aShowPicker is set to true, in which
   *        case the dialog only uses the folder identifier from the insertion
   *        point as the initially selected item in the folder picker.
   * @param [optional] aShowPicker
   *        see above
   * @return true if any transaction has been performed.
   */
  showAddFolderUI:
  function PUIU_showAddFolderUI(aTitle, aDefaultInsertionPoint, aShowPicker) {
    var info = {
      action: "add",
      type: "folder",
      hiddenRows: []
    };

    // allow default empty title
    if (typeof(aTitle) == "string")
      info.title = aTitle;

    if (aDefaultInsertionPoint) {
      info.defaultInsertionPoint = aDefaultInsertionPoint;
      if (!aShowPicker)
        info.hiddenRows.push("folderPicker");
    }
    return this._showBookmarkDialog(info);
  },

  /**
   * Shows the bookmark dialog corresponding to the specified info
   *
   * @param aInfo
   *        Describes the item to be edited/added in the dialog.
   *        See documentation at the top of bookmarkProperties.js
   * @param aMinimalUI
   *        [optional] if true, the dialog is opened by its alternative
   *        chrome: uri.
   *
   * @return true if any transaction has been performed, false otherwise.
   */
  _showBookmarkDialog: function PUIU__showBookmarkDialog(aInfo, aMinimalUI) {
    var dialogURL = aMinimalUI ?
                    "chrome://browser/content/places/bookmarkProperties2.xul" :
                    "chrome://browser/content/places/bookmarkProperties.xul";

    var features;
    if (aMinimalUI)
      features = "centerscreen,chrome,modal,resizable=yes";
    else
      features = "centerscreen,chrome,modal,resizable=no";
    this._getCurrentActiveWin().openDialog(dialogURL, "",  features, aInfo);
    return ("performed" in aInfo && aInfo.performed);
  },

  _getTopBrowserWin: function PUIU__getTopBrowserWin() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },

  _getCurrentActiveWin: function PUIU__getCurrentActiveWin() {
    return focusManager.activeWindow;
  },

  /**
   * Returns the closet ancestor places view for the given DOM node
   * @param aNode
   *        a DOM node
   * @return the closet ancestor places view if exists, null otherwsie.
   */
  getViewForNode: function PUIU_getViewForNode(aNode) {
    let node = aNode;

    // The view for a <menu> of which its associated menupopup is a places
    // view, is the menupopup.
    if (node.localName == "menu" && !node._placesNode &&
        node.lastChild._placesView)
      return node.lastChild._placesView;

    while (node instanceof Ci.nsIDOMElement) {
      if (node._placesView)
        return node._placesView;
      if (node.localName == "tree" && node.getAttribute("type") == "places")
        return node;

      node = node.parentNode;
    }

    return null;
  },

  /**
   * By calling this before visiting an URL, the visit will be associated to a
   * TRANSITION_TYPED transition (if there is no a referrer).
   * This is used when visiting pages from the history menu, history sidebar,
   * url bar, url autocomplete results, and history searches from the places
   * organizer.  If this is not called visits will be marked as
   * TRANSITION_LINK.
   */
  markPageAsTyped: function PUIU_markPageAsTyped(aURL) {
    PlacesUtils.history.QueryInterface(Ci.nsIBrowserHistory)
               .markPageAsTyped(this.createFixedURI(aURL));
  },

  /**
   * By calling this before visiting an URL, the visit will be associated to a
   * TRANSITION_BOOKMARK transition.
   * This is used when visiting pages from the bookmarks menu,
   * personal toolbar, and bookmarks from within the places organizer.
   * If this is not called visits will be marked as TRANSITION_LINK.
   */
  markPageAsFollowedBookmark: function PUIU_markPageAsFollowedBookmark(aURL) {
    PlacesUtils.history.markPageAsFollowedBookmark(this.createFixedURI(aURL));
  },

  /**
   * By calling this before visiting an URL, any visit in frames will be
   * associated to a TRANSITION_FRAMED_LINK transition.
   * This is actually used to distinguish user-initiated visits in frames
   * so automatic visits can be correctly ignored.
   */
  markPageAsFollowedLink: function PUIU_markPageAsUserClicked(aURL) {
    PlacesUtils.history.QueryInterface(Ci.nsIBrowserHistory)
               .markPageAsFollowedLink(this.createFixedURI(aURL));
  },

  /**
   * Allows opening of javascript/data URI only if the given node is
   * bookmarked (see bug 224521).
   * @param aURINode
   *        a URI node
   * @param aWindow
   *        a window on which a potential error alert is shown on.
   * @return true if it's safe to open the node in the browser, false otherwise.
   *
   */
  checkURLSecurity: function PUIU_checkURLSecurity(aURINode, aWindow) {
    if (PlacesUtils.nodeIsBookmark(aURINode))
      return true;

    var uri = PlacesUtils._uri(aURINode.uri);
    if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
      const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
      var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                           getService(Ci.nsIStringBundleService).
                           createBundle(BRANDING_BUNDLE_URI).
                           GetStringFromName("brandShortName");

      var errorStr = this.getString("load-js-data-url-error");
      Services.prompt.alert(aWindow, brandShortName, errorStr);
      return false;
    }
    return true;
  },

  /**
   * Get the description associated with a document, as specified in a <META>
   * element.
   * @param   doc
   *          A DOM Document to get a description for
   * @returns A description string if a META element was discovered with a
   *          "description" or "httpequiv" attribute, empty string otherwise.
   */
  getDescriptionFromDocument: function PUIU_getDescriptionFromDocument(doc) {
    var metaElements = doc.getElementsByTagName("META");
    for (var i = 0; i < metaElements.length; ++i) {
      if (metaElements[i].name.toLowerCase() == "description" ||
          metaElements[i].httpEquiv.toLowerCase() == "description") {
        return metaElements[i].content;
      }
    }
    return "";
  },

  /**
   * Retrieve the description of an item
   * @param aItemId
   *        item identifier
   * @returns the description of the given item, or an empty string if it is
   * not set.
   */
  getItemDescription: function PUIU_getItemDescription(aItemId) {
    if (PlacesUtils.annotations.itemHasAnnotation(aItemId, this.DESCRIPTION_ANNO))
      return PlacesUtils.annotations.getItemAnnotation(aItemId, this.DESCRIPTION_ANNO);
    return "";
  },

  /**
   * Gives the user a chance to cancel loading lots of tabs at once
   */
  _confirmOpenInTabs: function PUIU__confirmOpenInTabs(numTabsToOpen) {
    const WARN_ON_OPEN_PREF = "browser.tabs.warnOnOpen";
    var reallyOpen = true;

    if (Services.prefs.getBoolPref(WARN_ON_OPEN_PREF)) {
      if (numTabsToOpen >= Services.prefs.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
        // default to true: if it were false, we wouldn't get this far
        var warnOnOpen = { value: true };

        var messageKey = "tabs.openWarningMultipleBranded";
        var openKey = "tabs.openButtonMultiple";
        const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
        var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService).
                             createBundle(BRANDING_BUNDLE_URI).
                             GetStringFromName("brandShortName");

        var buttonPressed = Services.prompt.confirmEx(
          this._getCurrentActiveWin(),
          this.getString("tabs.openWarningTitle"),
          this.getFormattedString(messageKey, [numTabsToOpen, brandShortName]),
          (Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0) +
            (Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1),
          this.getString(openKey), null, null,
          this.getFormattedString("tabs.openWarningPromptMeBranded",
                                  [brandShortName]),
          warnOnOpen
        );

        reallyOpen = (buttonPressed == 0);
        // don't set the pref unless they press OK and it's false
        if (reallyOpen && !warnOnOpen.value)
          Services.prefs.setBoolPref(WARN_ON_OPEN_PREF, false);
      }
    }

    return reallyOpen;
  },

  /** aItemsToOpen needs to be an array of objects of the form:
    * {uri: string, isBookmark: boolean}
    */
  _openTabset: function PUIU__openTabset(aItemsToOpen, aEvent) {
    if (!aItemsToOpen.length)
      return;

    var urls = [];
    for (var i = 0; i < aItemsToOpen.length; i++) {
      var item = aItemsToOpen[i];
      if (item.isBookmark)
        this.markPageAsFollowedBookmark(item.uri);
      else
        this.markPageAsTyped(item.uri);

      urls.push(item.uri);
    }

    var browserWindow = this._getTopBrowserWin();
    var where = browserWindow ?
                browserWindow.whereToOpenLink(aEvent, false, true) : "window";
    if (where == "window") {
      let win = this._getCurrentActiveWin();
      win.openDialog(win.getBrowserURL(), "_blank",
                     "chrome,all,dialog=no", urls.join("|"));
      return;
    }

    var loadInBackground = where == "tabshifted" ? true : false;
    var replaceCurrentTab = where == "tab" ? false : true;
    browserWindow.gBrowser.loadTabs(urls, loadInBackground, replaceCurrentTab);
  },

  openContainerNodeInTabs: function PUIU_openContainerInTabs(aNode, aEvent) {
    var urlsToOpen = PlacesUtils.getURLsForContainerNode(aNode);
    if (!this._confirmOpenInTabs(urlsToOpen.length))
      return;

    this._openTabset(urlsToOpen, aEvent);
  },

  openURINodesInTabs: function PUIU_openURINodesInTabs(aNodes, aEvent) {
    var urlsToOpen = [];
    for (var i=0; i < aNodes.length; i++) {
      // skip over separators and folders
      if (PlacesUtils.nodeIsURI(aNodes[i]))
        urlsToOpen.push({uri: aNodes[i].uri, isBookmark: PlacesUtils.nodeIsBookmark(aNodes[i])});
    }
    this._openTabset(urlsToOpen, aEvent);
  },

  /**
   * Loads the node's URL in the appropriate tab or window or as a web
   * panel given the user's preference specified by modifier keys tracked by a
   * DOM mouse/key event.
   * @param   aNode
   *          An uri result node.
   * @param   aEvent
   *          The DOM mouse/key event with modifier keys set that track the
   *          user's preferred destination window or tab.
   */
  openNodeWithEvent: function PUIU_openNodeWithEvent(aNode, aEvent) {
    this.openNodeIn(aNode, this._getCurrentActiveWin().whereToOpenLink(aEvent));
  },

  /**
   * Loads the node's URL in the appropriate tab or window or as a
   * web panel.
   * see also openUILinkIn
   */
  openNodeIn: function PUIU_openNodeIn(aNode, aWhere) {
    if (aNode && PlacesUtils.nodeIsURI(aNode) &&
        this.checkURLSecurity(aNode, this._getCurrentActiveWin())) {
      var isBookmark = PlacesUtils.nodeIsBookmark(aNode);

      if (isBookmark)
        this.markPageAsFollowedBookmark(aNode.uri);
      else
        this.markPageAsTyped(aNode.uri);

      // Check whether the node is a bookmark which should be opened as
      // a web panel
      if (aWhere == "current" && isBookmark) {
        if (PlacesUtils.annotations
                       .itemHasAnnotation(aNode.itemId, this.LOAD_IN_SIDEBAR_ANNO)) {
          var browserWin = this._getTopBrowserWin();
          if (browserWin) {
            browserWin.openWebPanel(aNode.title, aNode.uri);
            return;
          }
        }
      }
      this._getCurrentActiveWin().openUILinkIn(aNode.uri, aWhere);
    }
  },

  /**
   * Helper for guessing scheme from an url string.
   * Used to avoid nsIURI overhead in frequently called UI functions.
   *
   * @param aUrlString the url to guess the scheme from.
   *
   * @return guessed scheme for this url string.
   *
   * @note this is not supposed be perfect, so use it only for UI purposes.
   */
  guessUrlSchemeForUI: function PUIU_guessUrlSchemeForUI(aUrlString) {
    return aUrlString.substr(0, aUrlString.indexOf(":"));
  },

  getBestTitle: function PUIU_getBestTitle(aNode) {
    var title;
    if (!aNode.title && PlacesUtils.uriTypes.indexOf(aNode.type) != -1) {
      // if node title is empty, try to set the label using host and filename
      // PlacesUtils._uri() will throw if aNode.uri is not a valid URI
      try {
        var uri = PlacesUtils._uri(aNode.uri);
        var host = uri.host;
        var fileName = uri.QueryInterface(Ci.nsIURL).fileName;
        // if fileName is empty, use path to distinguish labels
        title = host + (fileName ?
                        (host ? "/" + this.ellipsis + "/" : "") + fileName :
                        uri.path);
      }
      catch (e) {
        // Use (no title) for non-standard URIs (data:, javascript:, ...)
        title = "";
      }
    }
    else
      title = aNode.title;

    return title || this.getString("noTitle");
  },

  get leftPaneQueries() {
    // build the map
    this.leftPaneFolderId;
    return this.leftPaneQueries;
  },

  // Get the folder id for the organizer left-pane folder.
  get leftPaneFolderId() {
    let leftPaneRoot = -1;
    let allBookmarksId;

    // Shortcuts to services.
    let bs = PlacesUtils.bookmarks;
    let as = PlacesUtils.annotations;

    // This is the list of the left pane queries.
    let queries = {
      "PlacesRoot": { title: "" },
      "History": { title: this.getString("OrganizerQueryHistory") },
      "Tags": { title: this.getString("OrganizerQueryTags") },
      "AllBookmarks": { title: this.getString("OrganizerQueryAllBookmarks") },
      "BookmarksToolbar":
        { title: null,
          concreteTitle: PlacesUtils.getString("BookmarksToolbarFolderTitle"),
          concreteId: PlacesUtils.toolbarFolderId },
      "BookmarksMenu":
        { title: null,
          concreteTitle: PlacesUtils.getString("BookmarksMenuFolderTitle"),
          concreteId: PlacesUtils.bookmarksMenuFolderId },
      "UnfiledBookmarks":
        { title: null,
          concreteTitle: PlacesUtils.getString("UnsortedBookmarksFolderTitle"),
          concreteId: PlacesUtils.unfiledBookmarksFolderId },
    };
    // All queries but PlacesRoot.
    const EXPECTED_QUERY_COUNT = 6;

    // Removes an item and associated annotations, ignoring eventual errors.
    function safeRemoveItem(aItemId) {
      try {
        if (as.itemHasAnnotation(aItemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO) &&
            !(as.getItemAnnotation(aItemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO) in queries)) {
          // Some extension annotated their roots with our query annotation,
          // so we should not delete them.
          return;
        }
        // removeItemAnnotation does not check if item exists, nor the anno,
        // so this is safe to do.
        as.removeItemAnnotation(aItemId, PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
        as.removeItemAnnotation(aItemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO);
        // This will throw if the annotation is an orphan.
        bs.removeItem(aItemId);
      }
      catch(e) { /* orphan anno */ }
    }

    // Returns true if item really exists, false otherwise.
    function itemExists(aItemId) {
      try {
        bs.getItemIndex(aItemId);
        return true;
      }
      catch(e) {
        return false;
      }
    }

    // Get all items marked as being the left pane folder.
    let items = as.getItemsWithAnnotation(this.ORGANIZER_FOLDER_ANNO);
    if (items.length > 1) {
      // Something went wrong, we cannot have more than one left pane folder,
      // remove all left pane folders and continue.  We will create a new one.
      items.forEach(safeRemoveItem);
    }
    else if (items.length == 1 && items[0] != -1) {
      leftPaneRoot = items[0];
      // Check that organizer left pane root is valid.
      let version = as.getItemAnnotation(leftPaneRoot, this.ORGANIZER_FOLDER_ANNO);
      if (version != this.ORGANIZER_LEFTPANE_VERSION ||
          !itemExists(leftPaneRoot)) {
        // Invalid root, we must rebuild the left pane.
        safeRemoveItem(leftPaneRoot);
        leftPaneRoot = -1;
      }
    }

    if (leftPaneRoot != -1) {
      // A valid left pane folder has been found.
      // Build the leftPaneQueries Map.  This is used to quickly access them,
      // associating a mnemonic name to the real item ids.
      delete this.leftPaneQueries;
      this.leftPaneQueries = {};

      let items = as.getItemsWithAnnotation(this.ORGANIZER_QUERY_ANNO);
      // While looping through queries we will also check for their validity.
      let queriesCount = 0;
      for (let i = 0; i < items.length; i++) {
        let queryName = as.getItemAnnotation(items[i], this.ORGANIZER_QUERY_ANNO);

        // Some extension did use our annotation to decorate their items
        // with icons, so we should check only our elements, to avoid dataloss.
        if (!(queryName in queries))
          continue;

        let query = queries[queryName];
        query.itemId = items[i];

        if (!itemExists(query.itemId)) {
          // Orphan annotation, bail out and create a new left pane root.
          break;
        }

        // Check that all queries have valid parents.
        let parentId = bs.getFolderIdForItem(query.itemId);
        if (items.indexOf(parentId) == -1 && parentId != leftPaneRoot) {
          // The parent is not part of the left pane, bail out and create a new
          // left pane root.
          break;
        }

        // Titles could have been corrupted or the user could have changed his
        // locale.  Check title and eventually fix it.
        if (bs.getItemTitle(query.itemId) != query.title)
          bs.setItemTitle(query.itemId, query.title);
        if ("concreteId" in query) {
          if (bs.getItemTitle(query.concreteId) != query.concreteTitle)
            bs.setItemTitle(query.concreteId, query.concreteTitle);
        }

        // Add the query to our cache.
        this.leftPaneQueries[queryName] = query.itemId;
        queriesCount++;
      }

      if (queriesCount != EXPECTED_QUERY_COUNT) {
        // Queries number is wrong, so the left pane must be corrupt.
        // Note: we can't just remove the leftPaneRoot, because some query could
        // have a bad parent, so we have to remove all items one by one.
        items.forEach(safeRemoveItem);
        safeRemoveItem(leftPaneRoot);
      }
      else {
        // Everything is fine, return the current left pane folder.
        delete this.leftPaneFolderId;
        return this.leftPaneFolderId = leftPaneRoot;
      }
    }

    // Create a new left pane folder.
    var callback = {
      // Helper to create an organizer special query.
      create_query: function CB_create_query(aQueryName, aParentId, aQueryUrl) {
        let itemId = bs.insertBookmark(aParentId,
                                       PlacesUtils._uri(aQueryUrl),
                                       bs.DEFAULT_INDEX,
                                       queries[aQueryName].title);
        // Mark as special organizer query.
        as.setItemAnnotation(itemId, PlacesUIUtils.ORGANIZER_QUERY_ANNO, aQueryName,
                             0, as.EXPIRE_NEVER);
        // We should never backup this, since it changes between profiles.
        as.setItemAnnotation(itemId, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1,
                             0, as.EXPIRE_NEVER);
        // Add to the queries map.
        PlacesUIUtils.leftPaneQueries[aQueryName] = itemId;
        return itemId;
      },

      // Helper to create an organizer special folder.
      create_folder: function CB_create_folder(aFolderName, aParentId, aIsRoot) {
              // Left Pane Root Folder.
        let folderId = bs.createFolder(aParentId,
                                       queries[aFolderName].title,
                                       bs.DEFAULT_INDEX);
        // We should never backup this, since it changes between profiles.
        as.setItemAnnotation(folderId, PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO, 1,
                             0, as.EXPIRE_NEVER);
        // Disallow manipulating this folder within the organizer UI.
        bs.setFolderReadonly(folderId, true);

        if (aIsRoot) {
          // Mark as special left pane root.
          as.setItemAnnotation(folderId, PlacesUIUtils.ORGANIZER_FOLDER_ANNO,
                               PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION,
                               0, as.EXPIRE_NEVER);
        }
        else {
          // Mark as special organizer folder.
          as.setItemAnnotation(folderId, PlacesUIUtils.ORGANIZER_QUERY_ANNO, aFolderName,
                           0, as.EXPIRE_NEVER);
          PlacesUIUtils.leftPaneQueries[aFolderName] = folderId;
        }
        return folderId;
      },

      runBatched: function CB_runBatched(aUserData) {
        delete PlacesUIUtils.leftPaneQueries;
        PlacesUIUtils.leftPaneQueries = { };

        // Left Pane Root Folder.
        leftPaneRoot = this.create_folder("PlacesRoot", bs.placesRoot, true);

        // History Query.
        this.create_query("History", leftPaneRoot,
                          "place:type=" +
                          Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY +
                          "&sort=" +
                          Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING);

        // XXX: Downloads.

        // Tags Query.
        this.create_query("Tags", leftPaneRoot,
                          "place:type=" +
                          Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY +
                          "&sort=" +
                          Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);

        // All Bookmarks Folder.
        allBookmarksId = this.create_folder("AllBookmarks", leftPaneRoot, false);

        // All Bookmarks->Bookmarks Toolbar Query.
        this.create_query("BookmarksToolbar", allBookmarksId,
                          "place:folder=TOOLBAR");

        // All Bookmarks->Bookmarks Menu Query.
        this.create_query("BookmarksMenu", allBookmarksId,
                          "place:folder=BOOKMARKS_MENU");

        // All Bookmarks->Unfiled Bookmarks Query.
        this.create_query("UnfiledBookmarks", allBookmarksId,
                          "place:folder=UNFILED_BOOKMARKS");
      }
    };
    bs.runInBatchMode(callback, null);

    delete this.leftPaneFolderId;
    return this.leftPaneFolderId = leftPaneRoot;
  },

  /**
   * Get the folder id for the organizer left-pane folder.
   */
  get allBookmarksFolderId() {
    // ensure the left-pane root is initialized;
    this.leftPaneFolderId;
    delete this.allBookmarksFolderId;
    return this.allBookmarksFolderId = this.leftPaneQueries["AllBookmarks"];
  },

  /**
   * If an item is a left-pane query, returns the name of the query
   * or an empty string if not.
   *
   * @param aItemId id of a container
   * @returns the name of the query, or empty string if not a left-pane query
   */
  getLeftPaneQueryNameFromId: function PUIU_getLeftPaneQueryNameFromId(aItemId) {
    var queryName = "";
    // If the let pane hasn't been built, use the annotation service
    // directly, to avoid building the left pane too early.
    if (Object.getOwnPropertyDescriptor(this, "leftPaneFolderId").value === undefined) {
      try {
        queryName = PlacesUtils.annotations.
                                getItemAnnotation(aItemId, this.ORGANIZER_QUERY_ANNO);
      }
      catch (ex) {
        // doesn't have the annotation
        queryName = "";
      }
    }
    else {
      // If the left pane has already been built, use the name->id map
      // cached in PlacesUIUtils.
      for (let [name, id] in Iterator(this.leftPaneQueries)) {
        if (aItemId == id)
          queryName = name;
      }
    }
    return queryName;
  }
};

XPCOMUtils.defineLazyServiceGetter(PlacesUIUtils, "RDF",
                                   "@mozilla.org/rdf/rdf-service;1",
                                   "nsIRDFService");

XPCOMUtils.defineLazyGetter(PlacesUIUtils, "localStore", function() {
  return PlacesUIUtils.RDF.GetDataSource("rdf:local-store");
});

XPCOMUtils.defineLazyGetter(PlacesUIUtils, "ellipsis", function() {
  return Services.prefs.getComplexValue("intl.ellipsis",
                                        Ci.nsIPrefLocalizedString).data;
});

XPCOMUtils.defineLazyServiceGetter(PlacesUIUtils, "privateBrowsing",
                                   "@mozilla.org/privatebrowsing;1",
                                   "nsIPrivateBrowsingService");

XPCOMUtils.defineLazyServiceGetter(this, "URIFixup",
                                   "@mozilla.org/docshell/urifixup;1",
                                   "nsIURIFixup");

XPCOMUtils.defineLazyGetter(this, "bundle", function() {
  const PLACES_STRING_BUNDLE_URI =
    "chrome://browser/locale/places/places.properties";
  return Cc["@mozilla.org/intl/stringbundle;1"].
         getService(Ci.nsIStringBundleService).
         createBundle(PLACES_STRING_BUNDLE_URI);
});

XPCOMUtils.defineLazyServiceGetter(this, "focusManager",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");

/**
 * This is a compatibility shim for old PUIU.ptm users.
 *
 * If you're looking for transactions and writing new code using them, directly
 * use the transactions objects exported by the PlacesUtils.jsm module.
 *
 * This object will be removed once enough users are converted to the new API.
 */
XPCOMUtils.defineLazyGetter(PlacesUIUtils, "ptm", function() {
  // Ensure PlacesUtils is imported in scope.
  PlacesUtils;

  return {
    aggregateTransactions: function(aName, aTransactions)
      new PlacesAggregatedTransaction(aName, aTransactions),

    createFolder: function(aName, aContainer, aIndex, aAnnotations,
                           aChildItemsTransactions)
      new PlacesCreateFolderTransaction(aName, aContainer, aIndex, aAnnotations,
                                        aChildItemsTransactions),

    createItem: function(aURI, aContainer, aIndex, aTitle, aKeyword,
                         aAnnotations, aChildTransactions)
      new PlacesCreateBookmarkTransaction(aURI, aContainer, aIndex, aTitle,
                                          aKeyword, aAnnotations,
                                          aChildTransactions),

    createSeparator: function(aContainer, aIndex)
      new PlacesCreateSeparatorTransaction(aContainer, aIndex),

    createLivemark: function(aFeedURI, aSiteURI, aName, aContainer, aIndex,
                             aAnnotations)
      new PlacesCreateLivemarkTransaction(aFeedURI, aSiteURI, aName, aContainer,
                                          aIndex, aAnnotations),

    moveItem: function(aItemId, aNewContainer, aNewIndex)
      new PlacesMoveItemTransaction(aItemId, aNewContainer, aNewIndex),

    removeItem: function(aItemId)
      new PlacesRemoveItemTransaction(aItemId),

    editItemTitle: function(aItemId, aNewTitle)
      new PlacesEditItemTitleTransaction(aItemId, aNewTitle),

    editBookmarkURI: function(aItemId, aNewURI)
      new PlacesEditBookmarkURITransaction(aItemId, aNewURI),

    setItemAnnotation: function(aItemId, aAnnotationObject)
      new PlacesSetItemAnnotationTransaction(aItemId, aAnnotationObject),

    setPageAnnotation: function(aURI, aAnnotationObject)
      new PlacesSetPageAnnotationTransaction(aURI, aAnnotationObject),

    editBookmarkKeyword: function(aItemId, aNewKeyword)
      new PlacesEditBookmarkKeywordTransaction(aItemId, aNewKeyword),

    editBookmarkPostData: function(aItemId, aPostData)
      new PlacesEditBookmarkPostDataTransaction(aItemId, aPostData),

    editLivemarkSiteURI: function(aLivemarkId, aSiteURI)
      new PlacesEditLivemarkSiteURITransaction(aLivemarkId, aSiteURI),

    editLivemarkFeedURI: function(aLivemarkId, aFeedURI)
      new PlacesEditLivemarkFeedURITransaction(aLivemarkId, aFeedURI),

    editBookmarkMicrosummary: function(aItemId, aNewMicrosummary)
      new PlacesEditBookmarkMicrosummaryTransaction(aItemId, aNewMicrosummary),

    editItemDateAdded: function(aItemId, aNewDateAdded)
      new PlacesEditItemDateAddedTransaction(aItemId, aNewDateAdded),

    editItemLastModified: function(aItemId, aNewLastModified)
      new PlacesEditItemLastModifiedTransaction(aItemId, aNewLastModified),

    sortFolderByName: function(aFolderId)
      new PlacesSortFolderByNameTransaction(aFolderId),

    tagURI: function(aURI, aTags)
      new PlacesTagURITransaction(aURI, aTags),

    untagURI: function(aURI, aTags)
      new PlacesUntagURITransaction(aURI, aTags),

    /**
     * Transaction for setting/unsetting Load-in-sidebar annotation.
     *
     * @param aBookmarkId
     *        id of the bookmark where to set Load-in-sidebar annotation.
     * @param aLoadInSidebar
     *        boolean value.
     * @returns nsITransaction object.
     */
    setLoadInSidebar: function(aItemId, aLoadInSidebar)
    {
      let annoObj = { name: PlacesUIUtils.LOAD_IN_SIDEBAR_ANNO,
                      type: Ci.nsIAnnotationService.TYPE_INT32,
                      flags: 0,
                      value: aLoadInSidebar,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
      return new PlacesSetItemAnnotationTransaction(aItemId, annoObj);
    },

   /**
    * Transaction for editing the description of a bookmark or a folder.
    *
    * @param aItemId
    *        id of the item to edit.
    * @param aDescription
    *        new description.
    * @returns nsITransaction object.
    */
    editItemDescription: function(aItemId, aDescription)
    {
      let annoObj = { name: PlacesUIUtils.DESCRIPTION_ANNO,
                      type: Ci.nsIAnnotationService.TYPE_STRING,
                      flags: 0,
                      value: aDescription,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
      return new PlacesSetItemAnnotationTransaction(aItemId, annoObj);
    },

    ////////////////////////////////////////////////////////////////////////////
    //// nsITransactionManager forwarders.

    beginBatch: function()
      PlacesUtils.transactionManager.beginBatch(),

    endBatch: function()
      PlacesUtils.transactionManager.endBatch(),

    doTransaction: function(txn)
      PlacesUtils.transactionManager.doTransaction(txn),

    undoTransaction: function()
      PlacesUtils.transactionManager.undoTransaction(),

    redoTransaction: function()
      PlacesUtils.transactionManager.redoTransaction(),

    get numberOfUndoItems()
      PlacesUtils.transactionManager.numberOfUndoItems,
    get numberOfRedoItems()
      PlacesUtils.transactionManager.numberOfRedoItems,
    get maxTransactionCount()
      PlacesUtils.transactionManager.maxTransactionCount,
    set maxTransactionCount(val)
      PlacesUtils.transactionManager.maxTransactionCount = val,

    clear: function()
      PlacesUtils.transactionManager.clear(),

    peekUndoStack: function()
      PlacesUtils.transactionManager.peekUndoStack(),

    peekRedoStack: function()
      PlacesUtils.transactionManager.peekRedoStack(),

    getUndoStack: function()
      PlacesUtils.transactionManager.getUndoStack(),

    getRedoStack: function()
      PlacesUtils.transactionManager.getRedoStack(),

    AddListener: function(aListener)
      PlacesUtils.transactionManager.AddListener(aListener),

    RemoveListener: function(aListener)
      PlacesUtils.transactionManager.RemoveListener(aListener)
  }
});
