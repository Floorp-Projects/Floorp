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

function LOG(str) {
  dump("*** " + str + "\n");
}

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
const ORGANIZER_LEFTPANE_VERSION = 4;

#ifdef XP_MACOSX
// On Mac OSX, the transferable system converts "\r\n" to "\n\n", where we
// really just want "\n".
const NEWLINE= "\n";
#else
// On other platforms, the transferable system converts "\r\n" to "\n".
const NEWLINE = "\r\n";
#endif

function QI_node(aNode, aIID) {
  return aNode.QueryInterface(aIID);
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

  get ellipsis() {
    delete this.ellipsis;
    var pref = Cc["@mozilla.org/preferences-service;1"].
               getService(Ci.nsIPrefBranch);
    return this.ellipsis = pref.getComplexValue("intl.ellipsis",
                                                Ci.nsIPrefLocalizedString).data;
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
    if (aData.tags) {
      var tags = aData.tags.split(", ");
      // filter out tags already present, so that undo doesn't remove them
      // from pre-existing bookmarks
      var storedTags = PlacesUtils.tagging.getTagsForURI(itemURL, {});
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
  function PU__getFolderCopyTransaction(aData, aContainer, aIndex) {
    var self = this;
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
            txn = self._getLivemarkCopyTransaction(node, aContainer, index);
          else
            txn = self._getFolderCopyTransaction(node, aContainer, index);
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
        feedURI = PlacesUtils._uri(aAnno.value);
        return false;
      }
      else if (aAnno.name == LMANNO_SITEURI) {
        siteURI = PlacesUtils._uri(aAnno.value);
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
        if (copy)
          return this.ptm.createSeparator(container, index);
        // Move the separator otherwise
        return this.ptm.moveItem(data.id, container, index);
        break;
      default:
        if (type == PlacesUtils.TYPE_X_MOZ_URL ||
            type == PlacesUtils.TYPE_UNICODE) {
          var title = (type == PlacesUtils.TYPE_X_MOZ_URL) ? data.title :
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
  showAddBookmarkUI: function PU_showAddBookmarkUI(aURI,
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
  function PU_showMinimalAddBookmarkUI(aURI, aTitle, aDescription,
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
      // hide the Tags field if we are adding a keyword
      info.hiddenRows.push("tags");
      if (typeof(aPostData) == "string")
        info.postData = aPostData;
      if (typeof(aCharSet) == "string")
        info.charSet = aCharSet;
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
  function PU_showMinimalAddLivemarkURI(aFeedURI, aSiteURI, aTitle,
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
   * Opens the properties dialog for a given item identifier.
   *
   * @param aItemId
   *        item identifier for which the properties are to be shown
   * @param aType
   *        item type, either "bookmark" or "folder"
   * @return true if any transaction has been performed.
   */
  showItemProperties: function PU_showItemProperties(aItemId, aType) {
    var info = {
      action: "edit",
      type: aType,
      itemId: aItemId
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
  _showBookmarkDialog: function PU__showBookmarkDialog(aInfo, aMinimalUI) {
    var dialogURL = aMinimalUI ?
                    "chrome://browser/content/places/bookmarkProperties2.xul" :
                    "chrome://browser/content/places/bookmarkProperties.xul";

    var features;
    if (aMinimalUI)
      features = "centerscreen,chrome,dialog,resizable,modal";
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

    // the view for a <menu> of which its associated menupopup is a places view,
    // is the menupopup
    if (node.localName == "menu" && !node.node &&
        node.firstChild.getAttribute("type") == "places")
      return node.firstChild;

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
  createMenuItemForNode:
  function PUU_createMenuItemForNode(aNode, aContainersMap) {
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
        element.className = "menuitem-iconic bookmark-item";
      }
      else if (PlacesUtils.containerTypes.indexOf(type) != -1) {
        element = document.createElement("menu");
        element.setAttribute("container", "true");

        if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY) {
          element.setAttribute("query", "true");
          if (PlacesUtils.nodeIsTagQuery(aNode))
            element.setAttribute("tagContainer", "true");
          else if (PlacesUtils.nodeIsDay(aNode))
            element.setAttribute("dayContainer", "true");
          else if (PlacesUtils.nodeIsHost(aNode))
            element.setAttribute("hostContainer", "true");
        }
        else if (aNode.itemId != -1) {
          if (PlacesUtils.nodeIsLivemarkContainer(aNode))
            element.setAttribute("livemark", "true");
        }

        var popup = document.createElement("menupopup");
        popup.setAttribute("placespopup", "true");
        popup._resultNode = asContainer(aNode);
#ifdef XP_MACOSX
        // Binding on Mac native menus is lazy attached, so onPopupShowing,
        // in the capturing phase, fields are not yet initialized.
        // In that phase we have to ensure markers are not undefined to build
        // the popup correctly.
        popup._startMarker = -1;
        popup._endMarker = -1;
#else
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

      element.setAttribute("label", this.getBestTitle(aNode));

      if (iconURISpec)
        element.setAttribute("image", iconURISpec);
    }
    element.node = aNode;
    element.node.viewIndex = 0;

    return element;
  },

  cleanPlacesPopup: function PU_cleanPlacesPopup(aPopup) {
    // Remove places popup children and update markers to keep track of
    // their indices.
    var start = aPopup._startMarker != -1 ? aPopup._startMarker + 1 : 0;
    var end = aPopup._endMarker != -1 ? aPopup._endMarker :
                                        aPopup.childNodes.length;
    var items = [];
    var placesNodeFound = false;
    for (var i = start; i < end; ++i) {
      var item = aPopup.childNodes[i];
      if (item.getAttribute("builder") == "end") {
        // we need to do this for menus that have static content at the end but
        // are initially empty, eg. the history menu, we need to know where to
        // start inserting new items.
        aPopup._endMarker = i;
        break;
      }
      if (item.node) {
        items.push(item);
        placesNodeFound = true;
      }
      else {
        // This is static content...
        if (!placesNodeFound)
          // ...at the start of the popup
          // Initialized in menu.xml, in the base binding
          aPopup._startMarker++;
        else {
          // ...after places nodes
          aPopup._endMarker = i;
          break;
        }
      }
    }

    for (var i = 0; i < items.length; ++i) {
      aPopup.removeChild(items[i]);
      if (aPopup._endMarker != -1)
        aPopup._endMarker--;
    }
  },

  getBestTitle: function PU_getBestTitle(aNode) {
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

  // get the folder id for the organizer left-pane folder
  get leftPaneFolderId() {
    var leftPaneRoot = -1;
    var allBookmarksId;
    var items = PlacesUtils.annotations
                           .getItemsWithAnnotation(ORGANIZER_FOLDER_ANNO, {});
    if (items.length != 0 && items[0] != -1) {
      leftPaneRoot = items[0];
      // check organizer left pane version
      var version = PlacesUtils.annotations
                               .getItemAnnotation(leftPaneRoot, ORGANIZER_FOLDER_ANNO);
      if (version != ORGANIZER_LEFTPANE_VERSION) {
        // If version is not valid we must rebuild the left pane.
        PlacesUtils.bookmarks.removeFolder(leftPaneRoot);
        leftPaneRoot = -1;
      }
    }

    if (leftPaneRoot != -1) {
      // Build the leftPaneQueries Map
      delete this.leftPaneQueries;
      this.leftPaneQueries = {};
      var items = PlacesUtils.annotations
                             .getItemsWithAnnotation(ORGANIZER_QUERY_ANNO, {});
      for (var i=0; i < items.length; i++) {
        var queryName = PlacesUtils.annotations
                                   .getItemAnnotation(items[i], ORGANIZER_QUERY_ANNO);
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
        leftPaneRoot = PlacesUtils.bookmarks.createFolder(PlacesUtils.placesRootId, "", -1);
        // ensure immediate children can't be removed
        PlacesUtils.bookmarks.setFolderReadonly(leftPaneRoot, true);

        // History Query
        let uri = PlacesUtils._uri("place:sort=4&");
        let title = self.getString("OrganizerQueryHistory");
        let itemId = PlacesUtils.bookmarks.insertBookmark(leftPaneRoot, uri, -1, title);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "History", 0, EXPIRE_NEVER);
        self.leftPaneQueries["History"] = itemId;

        // XXX: Downloads

        // Tags Query
        uri = PlacesUtils._uri("place:type=" +
                          Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY +
                          "&sort=" +
                          Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);
        title = PlacesUtils.bookmarks.getItemTitle(PlacesUtils.tagsFolderId);
        itemId = PlacesUtils.bookmarks.insertBookmark(leftPaneRoot, uri, -1, title);
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

        // disallow manipulating this folder within the organizer UI
        PlacesUtils.bookmarks.setFolderReadonly(allBookmarksId, true);

        // All Bookmarks->Bookmarks Toolbar Query
        uri = PlacesUtils._uri("place:folder=TOOLBAR");
        itemId = PlacesUtils.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "BookmarksToolbar", 0, EXPIRE_NEVER);
        self.leftPaneQueries["BookmarksToolbar"] = itemId;

        // All Bookmarks->Bookmarks Menu Query
        uri = PlacesUtils._uri("place:folder=BOOKMARKS_MENU");
        itemId = PlacesUtils.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        PlacesUtils.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                                  "BookmarksMenu", 0, EXPIRE_NEVER);
        self.leftPaneQueries["BookmarksMenu"] = itemId;

        // All Bookmarks->Unfiled bookmarks
        uri = PlacesUtils._uri("place:folder=UNFILED_BOOKMARKS");
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
    PlacesUtils.annotations.setItemAnnotation(leftPaneRoot,
                                              ORGANIZER_FOLDER_ANNO,
                                              ORGANIZER_LEFTPANE_VERSION,
                                              0, EXPIRE_NEVER);
    delete this.leftPaneFolderId;
    return this.leftPaneFolderId = leftPaneRoot;
  },

  get allBookmarksFolderId() {
    // ensure the left-pane root is initialized;
    this.leftPaneFolderId;
    delete this.allBookmarksFolderId;
    return this.allBookmarksFolderId = this.leftPaneQueries["AllBookmarks"];
  },

  /**
  * Add, update or remove the livemark status menuitem.
  * @param aPopup
  *        The livemark container popup
  */
  ensureLivemarkStatusMenuItem:
  function PU_ensureLivemarkStatusMenuItem(aPopup) {
    var itemId = aPopup._resultNode.itemId;

    var lmStatus = null;
    if (PlacesUtils.annotations
                   .itemHasAnnotation(itemId, "livemark/loadfailed"))
      lmStatus = "bookmarksLivemarkFailed";
    else if (PlacesUtils.annotations
                        .itemHasAnnotation(itemId, "livemark/loading"))
      lmStatus = "bookmarksLivemarkLoading";

    if (lmStatus && !aPopup._lmStatusMenuItem) {
      // Create the status menuitem and cache it in the popup object.
      aPopup._lmStatusMenuItem = document.createElement("menuitem");
      aPopup._lmStatusMenuItem.setAttribute("lmStatus", lmStatus);
      aPopup._lmStatusMenuItem.setAttribute("label", this.getString(lmStatus));
      aPopup._lmStatusMenuItem.setAttribute("disabled", true);
      aPopup.insertBefore(aPopup._lmStatusMenuItem,
                          aPopup.childNodes[aPopup._startMarker + 1]);
      aPopup._startMarker++;
    }
    else if (lmStatus &&
             aPopup._lmStatusMenuItem.getAttribute("lmStatus") != lmStatus) {
      // Status has changed, update the cached status menuitem.
      aPopup._lmStatusMenuItem.setAttribute("label",
                                            this.getString(lmStatus));
    }
    else if (!lmStatus && aPopup._lmStatusMenuItem){
      // No status, remove the cached menuitem.
      aPopup.removeChild(aPopup._lmStatusMenuItem);
      aPopup._lmStatusMenuItem = null;
      aPopup._startMarker--;
    }
  }
};
