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

function LOG(str) {
  dump("*** " + str + "\n");
}
LOG("loading utils.js");

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;

__defineGetter__("PlacesUtils", function() {
  delete this.PlacesUtils
  var tmpScope = {};
  Components.utils.import("resource://gre/modules/utils.js", tmpScope);
  return this.PlacesUtils = tmpScope.PlacesUtils;
});

const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LMANNO_FEEDURI = "livemark/feedURI";
const LMANNO_SITEURI = "livemark/siteURI";
const ORGANIZER_FOLDER_ANNO = "PlacesOrganizer/OrganizerFolder";
const ORGANIZER_QUERY_ANNO = "PlacesOrganizer/OrganizerQuery";

#ifdef XP_MACOSX
// On Mac OSX, the transferable system converts "\r\n" to "\n\n", where we
// really just want "\n".
const NEWLINE= "\n";
#else
// On other platforms, the transferable system converts "\r\n" to "\n".
const NEWLINE = "\r\n";
#endif

function QI_node(aNode, aIID) {
  var result = null;
  try {
    result = aNode.QueryInterface(aIID);
  }
  catch (e) {
  }
  NS_ASSERT(result, "Node QI Failed");
  return result;
}
function asVisit(aNode)    { return QI_node(aNode, Ci.nsINavHistoryVisitResultNode);    }
function asFullVisit(aNode){ return QI_node(aNode, Ci.nsINavHistoryFullVisitResultNode);}
function asContainer(aNode){ return QI_node(aNode, Ci.nsINavHistoryContainerResultNode);}
function asQuery(aNode)    { return QI_node(aNode, Ci.nsINavHistoryQueryResultNode);    }

var PlacesUIUtils = {
  /**
   * The Microsummary Service
   */
  get microsummaries() {
    delete this.microsummaries;
    return this.microsummaries = Cc["@mozilla.org/microsummary/service;1"].
                                 getService(Ci.nsIMicrosummaryService);
  },

  get RDF() {
    delete this.RDF;
    return this.RDF = Cc["@mozilla.org/rdf/rdf-service;1"].
                      getService(Ci.nsIRDFService);
  },

  get localStore() {
    delete this.localStore;
    return this.localStore = this.RDF.GetDataSource("rdf:local-store");
  },

  get ptm() {
    delete this.ptm;
    return this.ptm = Cc["@mozilla.org/browser/placesTransactionsService;1"].
                      getService(Ci.nsIPlacesTransactionsService);
  },

  get clipboard() {
    delete this.clipboard;
    return this.clipboard = Cc["@mozilla.org/widget/clipboard;1"].
                            getService(Ci.nsIClipboard);
  },

  get URIFixup() {
    delete this.URIFixup;
    return this.URIFixup = Cc["@mozilla.org/docshell/urifixup;1"].
                           getService(Ci.nsIURIFixup);
  },

  /**
   * Makes a URI from a spec, and do fixup
   * @param   aSpec
   *          The string spec of the URI
   * @returns A URI object for the spec.
   */
  createFixedURI: function PU_createFixedURI(aSpec) {
    return this.URIFixup.createFixupURI(aSpec, 0);
  },

  /**
   * Wraps a string in a nsISupportsString wrapper
   * @param   aString
   *          The string to wrap
   * @returns A nsISupportsString object containing a string.
   */
  _wrapString: function PU__wrapString(aString) {
    var s = Cc["@mozilla.org/supports-string;1"].
            createInstance(Ci.nsISupportsString);
    s.data = aString;
    return s;
  },

  /**
   * String bundle helpers
   */
  get _bundle() {
    const PLACES_STRING_BUNDLE_URI =
        "chrome://browser/locale/places/places.properties";
    delete this._bundle;
    return this._bundle = Cc["@mozilla.org/intl/stringbundle;1"].
                          getService(Ci.nsIStringBundleService).
                          createBundle(PLACES_STRING_BUNDLE_URI);
  },

  getFormattedString: function PU_getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },

  getString: function PU_getString(key) {
    return this._bundle.GetStringFromName(key);
  },

  /**
   * Get a transaction for copying a uri item from one container to another
   * as a bookmark.
   * @param   aURI
   *          The URI of the item being copied
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
   * @param   aID
   *          The identifier of the bookmark item being copied
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
  function PU__getBookmarkItemCopyTransaction(aData, aContainer, aIndex,
                                              aExcludeAnnotations) {
    var itemURL = PlacesUtils._uri(aData.uri);
    var itemTitle = aData.title;
    var keyword = aData.keyword || null;
    var annos = aData.annos || [];
    if (aExcludeAnnotations) {
      annos = annos.filter(function(aValue, aIndex, aArray) {
        return aExcludeAnnotations.indexOf(aValue.name) == -1;
      });
    }
    var childTxns = [];
    if (aData.dateAdded)
      childTxns.push(this.ptm.editItemDateAdded(null, aData.dateAdded));
    if (aData.lastModified)
      childTxns.push(this.ptm.editItemLastModified(null, aData.lastModified));

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
  function PU__getFolderCopyTransaction(aData, aContainer, aIndex) {
    var self = this;
    function getChildItemsTransactions(aChildren) {
      var childItemsTransactions = [];
      var cc = aChildren.length;
      var index = aIndex;
      for (var i = 0; i < cc; ++i) {
        var txn = null;
        var node = aChildren[i];

        // adjusted to make sure that items are given the correct index -
        // transactions insert differently if index == -1
        if (aIndex > -1)
          index = aIndex + i;

        if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
          if (node.livemark && node.annos) // node is a livemark
            txn = self._getLivemarkCopyTransaction(node, aContainer, index);
          else {
            var folderItemsTransactions = [];
            if (node.dateAdded)
              folderItemsTransactions.push(self.ptm.editItemDateAdded(null, node.dateAdded));
            if (node.lastModified)
              folderItemsTransactions.push(self.ptm.editItemLastModified(null, node.lastModified));
            var annos = node.annos || [];
            txn = self.ptm.createFolder(node.title, -1, index, annos,
                                        folderItemsTransactions);
          }
        }
        else if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR)
          txn = self.ptm.createSeparator(-1, index);
        else if (node.type == PlacesUtils.TYPE_X_MOZ_PLACE)
          txn = self._getBookmarkItemCopyTransaction(node, -1, index);

        NS_ASSERT(txn, "Unexpected item under a bookmarks folder");
        if (txn)
          childItemsTransactions.push(txn);
      }
      return childItemsTransactions;
    }

    // tag folders use tag transactions
    if (aContainer == PlacesUtils.bookmarks.tagsFolder) {
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
      return this.ptm.createFolder(aData.title, aContainer, aIndex, annos, childItems);
    }
  },

  _getLivemarkCopyTransaction:
  function PU__getLivemarkCopyTransaction(aData, aContainer, aIndex) {
    NS_ASSERT(aData.livemark && aData.annos, "node is not a livemark");
    // Place is a Livemark Container
    var feedURI = null;
    var siteURI = null;
    aData.annos = aData.annos.filter(function(aAnno) {
      if (aAnno.name == LMANNO_FEEDURI) {
        feedURI = this._uri(aAnno.value);
        return false;
      }
      else if (aAnno.name == LMANNO_SITEURI) {
        siteURI = this._uri(aAnno.value);
        return false;
      }
      return true;
    }, this);
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
  makeTransaction: function PU_makeTransaction(data, type, container,
                                               index, copy) {
    switch (data.type) {
      case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
        if (copy)
          return this._getFolderCopyTransaction(data, container, index);
        else { // Move the item
          var id = data.folder ? data.folder.id : data.id;
          return this.ptm.moveItem(id, container, index);
        }
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE:
        if (data.id <= 0) // non-bookmark item
          return this._getURIItemCopyTransaction(data, container, index);
  
        if (copy) {
          // Copying a child of a live-bookmark by itself should result
          // as a new normal bookmark item (bug 376731)
          var copyBookmarkAnno =
            this._getBookmarkItemCopyTransaction(data, container, index,
                                                 ["livemark/bookmarkFeedURI"]);
          return copyBookmarkAnno;
        }
        else
          return this.ptm.moveItem(data.id, container, index);
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
        // There is no data in a separator, so copying it just amounts to
        // inserting a new separator.
        return this.ptm.createSeparator(container, index);
        break;
      default:
        if (type == PlacesUtils.TYPE_X_MOZ_URL || type == PlacesUtils.TYPE_UNICODE) {
          var title = (type == PlacesUtils.TYPE_X_MOZ_URL) ? data.title : data.uri;
          return this.ptm.createItem(PlacesUtils._uri(data.uri), container, index,
                                     title);
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
   * @return true if any transaction has been performed.
   *
   * Notes:
   *  - the location, description and "load in sidebar" fields are
   *    visible only if there is no initial URI (aURI is null).
   *  - When aDefaultInsertionPoint is not set, the dialog defaults to the
   *    bookmarks root folder.
   */
  showAddBookmarkUI: function PU_showAddBookmarkUI(aURI,
                                                   aTitle,
                                                   aDescription,
                                                   aDefaultInsertionPoint,
                                                   aShowPicker,
                                                   aLoadInSidebar,
                                                   aKeyword,
                                                   aPostData) {
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
        info.hiddenRows = ["folder picker"];
    }

    if (aLoadInSidebar)
      info.loadBookmarkInSidebar = true;

    if (typeof(aKeyword) == "string") {
      info.keyword = aKeyword;
      if (typeof(aPostData) == "string")
        info.postData = aPostData;
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
  function PU_showMinimalAddBookmarkUI(aURI, aTitle, aDescription,
                                       aDefaultInsertionPoint, aShowPicker,
                                       aLoadInSidebar, aKeyword, aPostData) {
    var info = {
      action: "add",
      type: "bookmark",
      hiddenRows: ["location", "description", "load in sidebar"]
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
        info.hiddenRows.push("folder picker");
    }

    if (aLoadInSidebar)
      info.loadBookmarkInSidebar = true;

    if (typeof(aKeyword) == "string") {
      info.keyword = aKeyword;
      if (typeof(aPostData) == "string")
        info.postData = aPostData;
    }
    else
      info.hiddenRows.push("keyword");

    this._showBookmarkDialog(info, true);
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
  showAddLivemarkUI: function PU_showAddLivemarkURI(aFeedURI,
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
        info.hiddenRows = ["folder picker"];
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
  function PU_showMinimalAddLivemarkURI(aFeedURI, aSiteURI, aTitle,
                                        aDescription, aDefaultInsertionPoint,
                                        aShowPicker) {
    var info = {
      action: "add",
      type: "livemark",
      hiddenRows: ["feedURI", "siteURI", "description"]
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
        info.hiddenRows.push("folder picker");
    }
    this._showBookmarkDialog(info, true);
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
  showMinimalAddMultiBookmarkUI: function PU_showAddMultiBookmarkUI(aURIList) {
    NS_ASSERT(aURIList.length,
              "showAddMultiBookmarkUI expects a list of nsIURI objects");
    var info = {
      action: "add",
      type: "folder",
      hiddenRows: ["description"],
      URIList: aURIList
    };
    this._showBookmarkDialog(info, true);
  },

  /**
   * Opens the bookmark properties panel for a given bookmark identifier.
   *
   * @param aId
   *        bookmark identifier for which the properties are to be shown
   * @return true if any transaction has been performed.
   */
  showBookmarkProperties: function PU_showBookmarkProperties(aId) {
    var info = {
      action: "edit",
      type: "bookmark",
      bookmarkId: aId
    };
    return this._showBookmarkDialog(info);
  },

  /**
   * Opens the folder properties panel for a given folder ID.
   *
   * @param aId
   *        an integer representing the ID of the folder to edit
   * @return true if any transaction has been performed.
   */
  showFolderProperties: function PU_showFolderProperties(aId) {
    var info = {
      action: "edit",
      type: "folder",
      folderId: aId
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
  function PU_showAddFolderUI(aTitle, aDefaultInsertionPoint, aShowPicker) {
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
        info.hiddenRows.push("folder picker");
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
   * Note: In minimal UI mode, we open the dialog non-modal on any system but
   *       Mac OS X.
   * @return true if any transaction has been performed, false otherwise.
   * Note: the return value of this method is not reliable in minimal UI mode
   * since the dialog may not be opened modally.
   */
  _showBookmarkDialog: function PU__showBookmarkDialog(aInfo, aMinimalUI) {
    var dialogURL = aMinimalUI ?
                    "chrome://browser/content/places/bookmarkProperties2.xul" :
                    "chrome://browser/content/places/bookmarkProperties.xul";

    var features;
    if (aMinimalUI)
#ifdef XP_MACOSX
      features = "centerscreen,chrome,dialog,resizable,modal";
#else
      features = "centerscreen,chrome,dialog,resizable,dependent";
#endif
    else
      features = "centerscreen,chrome,modal,resizable=no";
    window.openDialog(dialogURL, "",  features, aInfo);
    return ("performed" in aInfo && aInfo.performed);
  },

  /**
   * Returns the closet ancestor places view for the given DOM node
   * @param aNode
   *        a DOM node
   * @return the closet ancestor places view if exists, null otherwsie.
   */
  getViewForNode: function PU_getViewForNode(aNode) {
    var node = aNode;
    while (node) {
      // XXXmano: Use QueryInterface(nsIPlacesView) once we implement it...
      if (node.getAttribute("type") == "places")
        return node;

      node = node.parentNode;
    }

    return null;
  },

  /**
   * By calling this before we visit a URL, we will use TRANSITION_TYPED
   * as the transition for the visit to that URL (if we don't have a referrer).
   * This is used when visiting pages from the history menu, history sidebar,
   * url bar, url autocomplete results, and history searches from the places
   * organizer.  If we don't call this, we'll treat those visits as
   * TRANSITION_LINK.
   */
  markPageAsTyped: function PU_markPageAsTyped(aURL) {
    PlacesUtils.history.QueryInterface(Ci.nsIBrowserHistory)
               .markPageAsTyped(this.createFixedURI(aURL));
  },

  /**
   * By calling this before we visit a URL, we will use TRANSITION_BOOKMARK
   * as the transition for the visit to that URL (if we don't have a referrer).
   * This is used when visiting pages from the bookmarks menu, 
   * personal toolbar, and bookmarks from within the places organizer.
   * If we don't call this, we'll treat those visits as TRANSITION_LINK.
   */
  markPageAsFollowedBookmark: function PU_markPageAsFollowedBookmark(aURL) {
    PlacesUtils.history.markPageAsFollowedBookmark(this.createFixedURI(aURL));
  },

  /**
   * Allows opening of javascript/data URI only if the given node is
   * bookmarked (see bug 224521).
   * @param aURINode
   *        a URI node
   * @return true if it's safe to open the node in the browser, false otherwise.
   *
   */
  checkURLSecurity: function PU_checkURLSecurity(aURINode) {
    if (!PlacesUtils.nodeIsBookmark(aURINode)) {
      var uri = PlacesUtils._uri(aURINode.uri);
      if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
        const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
        var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService).
                             createBundle(BRANDING_BUNDLE_URI).
                             GetStringFromName("brandShortName");
        var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                            getService(Ci.nsIPromptService);

        var errorStr = this.getString("load-js-data-url-error");
        promptService.alert(window, brandShortName, errorStr);
        return false;
      }
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
  getDescriptionFromDocument: function PU_getDescriptionFromDocument(doc) {
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
  getItemDescription: function PU_getItemDescription(aItemId) {
    if (PlacesUtils.annotations.itemHasAnnotation(aItemId, DESCRIPTION_ANNO))
      return PlacesUtils.annotations.getItemAnnotation(aItemId, DESCRIPTION_ANNO);
    return "";
  },

  /**
   * Gives the user a chance to cancel loading lots of tabs at once
   */
  _confirmOpenInTabs: function PU__confirmOpenInTabs(numTabsToOpen) {
    var pref = Cc["@mozilla.org/preferences-service;1"].
               getService(Ci.nsIPrefBranch);

    const kWarnOnOpenPref = "browser.tabs.warnOnOpen";
    var reallyOpen = true;
    if (pref.getBoolPref(kWarnOnOpenPref)) {
      if (numTabsToOpen >= pref.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
        var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                            getService(Ci.nsIPromptService);

        // default to true: if it were false, we wouldn't get this far
        var warnOnOpen = { value: true };

        var messageKey = "tabs.openWarningMultipleBranded";
        var openKey = "tabs.openButtonMultiple";
        const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
        var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService).
                             createBundle(BRANDING_BUNDLE_URI).
                             GetStringFromName("brandShortName");

        var buttonPressed = promptService.confirmEx(window,
          this.getString("tabs.openWarningTitle"),
          this.getFormattedString(messageKey, [numTabsToOpen, brandShortName]),
          (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0)
           + (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
          this.getString(openKey), null, null,
          this.getFormattedString("tabs.openWarningPromptMeBranded",
                                  [brandShortName]), warnOnOpen);

        reallyOpen = (buttonPressed == 0);
        // don't set the pref unless they press OK and it's false
        if (reallyOpen && !warnOnOpen.value)
          pref.setBoolPref(kWarnOnOpenPref, false);
      }
    }
    return reallyOpen;
  },

  /** aItemsToOpen needs to be an array of objects of the form:
    * {uri: string, isBookmark: boolean}
    */
  _openTabset: function PU__openTabset(aItemsToOpen, aEvent) {
    var urls = [];
    for (var i = 0; i < aItemsToOpen.length; i++) {
      var item = aItemsToOpen[i];
      if (item.isBookmark)
        this.markPageAsFollowedBookmark(item.uri);
      else
        this.markPageAsTyped(item.uri);

      urls.push(item.uri);
    }

    var browserWindow = getTopWin();
    var where = browserWindow ?
                whereToOpenLink(aEvent, false, true) : "window";
    if (where == "window") {
      window.openDialog(getBrowserURL(), "_blank",
                        "chrome,all,dialog=no", urls.join("|"));
      return;
    }

    var loadInBackground = where == "tabshifted" ? true : false;
    var replaceCurrentTab = where == "tab" ? false : true;
    browserWindow.getBrowser().loadTabs(urls, loadInBackground,
                                        replaceCurrentTab);
  },

  openContainerNodeInTabs: function PU_openContainerInTabs(aNode, aEvent) {
    var urlsToOpen = PlacesUtils.getURLsForContainerNode(aNode);
    if (!this._confirmOpenInTabs(urlsToOpen.length))
      return;

    this._openTabset(urlsToOpen, aEvent);
  },

  openURINodesInTabs: function PU_openURINodesInTabs(aNodes, aEvent) {
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
  openNodeWithEvent: function PU_openNodeWithEvent(aNode, aEvent) {
    this.openNodeIn(aNode, whereToOpenLink(aEvent));
  },
  
  /**
   * Loads the node's URL in the appropriate tab or window or as a
   * web panel.
   * see also openUILinkIn
   */
  openNodeIn: function PU_openNodeIn(aNode, aWhere) {
    if (aNode && PlacesUtils.nodeIsURI(aNode) &&
        this.checkURLSecurity(aNode)) {
      var isBookmark = PlacesUtils.nodeIsBookmark(aNode);

      if (isBookmark)
        this.markPageAsFollowedBookmark(aNode.uri);
      else
        this.markPageAsTyped(aNode.uri);

      // Check whether the node is a bookmark which should be opened as
      // a web panel
      if (aWhere == "current" && isBookmark) {
        if (PlacesUtils.annotations
                       .itemHasAnnotation(aNode.itemId, LOAD_IN_SIDEBAR_ANNO)) {
          var w = getTopWin();
          if (w) {
            w.openWebPanel(aNode.title, aNode.uri);
            return;
          }
        }
      }
      openUILinkIn(aNode.uri, aWhere);
    }
  },

  /**
   * Helper for the toolbar and menu views
   */
  createMenuItemForNode: function(aNode, aContainersMap) {
    var element;
    var type = aNode.type;
    if (type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR)
      element = document.createElement("menuseparator");
    else {
      var iconURI = aNode.icon;
      var iconURISpec = "";
      if (iconURI)
        iconURISpec = iconURI.spec;

      if (PlacesUtils.uriTypes.indexOf(type) != -1) {
        element = document.createElement("menuitem");
        element.setAttribute("statustext", aNode.uri);
        element.className = "menuitem-iconic bookmark-item";
      }
      else if (PlacesUtils.containerTypes.indexOf(type) != -1) {
        element = document.createElement("menu");
        element.setAttribute("container", "true");

        if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY)
          element.setAttribute("query", "true");
        else if (aNode.itemId != -1) {
          if (PlacesUtils.nodeIsLivemarkContainer(aNode))
            element.setAttribute("livemark", "true");
          else if (PlacesUtils.bookmarks.
                               getFolderIdForItem(aNode.itemId) == PlacesUtils.tagsFolderId)
            element.setAttribute("tagContainer", "true");
        }

        var popup = document.createElement("menupopup");
        popup.setAttribute("placespopup", "true");
        popup._resultNode = asContainer(aNode);
#ifndef XP_MACOSX
        // no context menu on mac
        popup.setAttribute("context", "placesContext");
#endif
        element.appendChild(popup);
        if (aContainersMap)
          aContainersMap.push({ resultNode: aNode, domNode: popup });
        element.className = "menu-iconic bookmark-item";
      }
      else
        throw "Unexpected node";

      element.setAttribute("label", aNode.title);
      if (iconURISpec)
        element.setAttribute("image", iconURISpec);
    }
    element.node = aNode;
    element.node.viewIndex = 0;

    return element;
  },

  get leftPaneQueries() {    
    // build the map
    this.leftPaneFolderId;
    return this.leftPaneQueries;
  },

  // get the folder id for the organizer left-pane folder
  get leftPaneFolderId() {
    var leftPaneRoot = -1;
    var allBookmarksId;
    var items = PlacesUtils.annotations.getItemsWithAnnotation(ORGANIZER_FOLDER_ANNO, {});
    if (items.length != 0 && items[0] != -1)
      leftPaneRoot = items[0];
    if (leftPaneRoot != -1) {
      // Build the leftPaneQueries Map
      delete this.leftPaneQueries;
      this.leftPaneQueries = {};
      var items = PlacesUtils.annotations.
                              getItemsWithAnnotation(ORGANIZER_QUERY_ANNO, { });
      for (var i=0; i < items.length; i++) {
        var queryName = PlacesUtils.annotations.
                                    getItemAnnotation(items[i], ORGANIZER_QUERY_ANNO);
        this.leftPaneQueries[queryName] = items[i];
      }
      delete this.leftPaneFolderId;
      return this.leftPaneFolderId = leftPaneRoot;
    }

    var self = this;
    const EXPIRE_NEVER = PlacesUtils.annotations.EXPIRE_NEVER;
    var callback = {
      runBatched: function(aUserData) {
        delete self.leftPaneQueries;
        self.leftPaneQueries = { };

        // Left Pane Root Folder
        leftPaneRoot =
          PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId, "", -1);

        // History Query
        let uri = PlacesUtils._uri("place:sort=4&");
        let title = self.getString("OrganizerQueryHistory");
        let itemId = PlacesUtils.bookmarks.insertBookmark(leftPaneRoot, uri, -1, title);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "History", 0, EXPIRE_NEVER);
        self.leftPaneQueries["History"] = itemId;

        // XXX: Downloads

        // Tags Query
        uri = PlacesUtils._uri("place:folder=" + PlacesUtils.tagsFolderId);
        itemId = PlacesUtils.bookmarks.insertBookmark(leftPaneRoot, uri, -1, null);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "Tags", 0, EXPIRE_NEVER);
        self.leftPaneQueries["Tags"] = itemId;

        // All Bookmarks Folder
        title = self.getString("OrganizerQueryAllBookmarks");
        itemId = PlacesUtils.bookmarks.createFolder(leftPaneRoot, title, -1);
        allBookmarksId = itemId;
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "AllBookmarks", 0, EXPIRE_NEVER);
        self.leftPaneQueries["AllBookmarks"] = itemId;

        // All Bookmarks->Bookmarks Toolbar Query
        uri = PlacesUtils._uri("place:folder=" + PlacesUtils.toolbarFolderId);
        itemId = PlacesUtils.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "BookmarksToolbar", 0, EXPIRE_NEVER);
        self.leftPaneQueries["BookmarksToolbar"] = itemId;

        // All Bookmarks->Bookmarks Menu Query
        uri = PlacesUtils._uri("place:folder=" + PlacesUtils.bookmarksMenuFolderId);
        itemId = PlacesUtils.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "BookmarksMenu", 0, EXPIRE_NEVER);
        self.leftPaneQueries["BookmarksMenu"] = itemId;

        // All Bookmarks->Unfiled bookmarks
        uri = PlacesUtils._uri("place:folder=" + PlacesUtils.unfiledBookmarksFolderId);
        itemId = PlacesUtils.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "UnfiledBookmarks", 0,
                                                  EXPIRE_NEVER);
        self.leftPaneQueries["UnfiledBookmarks"] = itemId;

        // disallow manipulating this folder within the organizer UI
        PlacesUtils.bookmarks.setFolderReadonly(leftPaneRoot, true);
      }
    };
    PlacesUtils.bookmarks.runInBatchMode(callback, null);
    PlacesUtils.annotations.setItemAnnotation(leftPaneRoot, ORGANIZER_FOLDER_ANNO,
                                              true, 0, EXPIRE_NEVER);
    delete this.leftPaneFolderId;
    return this.leftPaneFolderId = leftPaneRoot;
  },

  get allBookmarksFolderId() {
    // ensure the left-pane root is initialized;
    this.leftPaneFolderId;
    delete this.allBookmarksFolderId;
    return this.allBookmarksFolderId = this.leftPaneQueries["AllBookmarks"];
  }
};

PlacesUIUtils.placesFlavors = [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                               PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
                               PlacesUtils.TYPE_X_MOZ_PLACE];

PlacesUIUtils.GENERIC_VIEW_DROP_TYPES = [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                         PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
                                         PlacesUtils.TYPE_X_MOZ_PLACE,
                                         PlacesUtils.TYPE_X_MOZ_URL,
                                         PlacesUtils.TYPE_UNICODE];
LOG("loaded utils.js");
