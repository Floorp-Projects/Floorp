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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const DESCRIPTION_ANNO = "bookmarkProperties/description";

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
function asFolder(aNode)   { return QI_node(aNode, Ci.nsINavHistoryFolderResultNode);   }
function asVisit(aNode)    { return QI_node(aNode, Ci.nsINavHistoryVisitResultNode);    }
function asFullVisit(aNode){ return QI_node(aNode, Ci.nsINavHistoryFullVisitResultNode);}
function asContainer(aNode){ return QI_node(aNode, Ci.nsINavHistoryContainerResultNode);}
function asQuery(aNode)    { return QI_node(aNode, Ci.nsINavHistoryQueryResultNode);    }

var PlacesUtils = {
  // Place entries that are containers, e.g. bookmark folders or queries. 
  TYPE_X_MOZ_PLACE_CONTAINER: "text/x-moz-place-container",
  // Place entries that are bookmark separators.
  TYPE_X_MOZ_PLACE_SEPARATOR: "text/x-moz-place-separator",
  // Place entries that are not containers or separators
  TYPE_X_MOZ_PLACE: "text/x-moz-place",
  // Place entries in shortcut url format (url\ntitle)
  TYPE_X_MOZ_URL: "text/x-moz-url",
  // Place entries formatted as HTML anchors
  TYPE_HTML: "text/html",
  // Place entries as raw URL text
  TYPE_UNICODE: "text/unicode",

  /**
   * The Bookmarks Service.
   */
  _bookmarks: null,
  get bookmarks() {
    if (!this._bookmarks) {
      this._bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                        getService(Ci.nsINavBookmarksService);
    }
    return this._bookmarks;
  },

  /**
   * The Nav History Service.
   */
  _history: null,
  get history() {
    if (!this._history) {
      this._history = Cc["@mozilla.org/browser/nav-history-service;1"].
                      getService(Ci.nsINavHistoryService);
    }
    return this._history;
  },

  /**
   * The Live Bookmark Service.
   */
  _livemarks: null,
  get livemarks() {
    if (!this._livemarks) {
      this._livemarks = Cc["@mozilla.org/browser/livemark-service;2"].
                        getService(Ci.nsILivemarkService);
    }
    return this._livemarks;
  },

  /**
   * The Annotations Service.
   */
  _annotations: null,
  get annotations() {
    if (!this._annotations) {
      this._annotations = Cc["@mozilla.org/browser/annotation-service;1"].
                          getService(Ci.nsIAnnotationService);
    }
    return this._annotations;
  },

  /**
   * The Transaction Manager for this window.
   */
  _tm: null,
  get tm() {
    if (!this._tm) {
      this._tm = Cc["@mozilla.org/transactionmanager;1"].
                 createInstance(Ci.nsITransactionManager);
    }
    return this._tm;
  },

  /**
   * Makes a URI from a spec.
   * @param   aSpec
   *          The string spec of the URI
   * @returns A URI object for the spec.
   */
  _uri: function PU__uri(aSpec) {
    NS_ASSERT(aSpec, "empty URL spec");
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    return ios.newURI(aSpec, null, null);
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
  __bundle: null,
  get _bundle() {
    if (!this.__bundle) {
      const PLACES_STRING_BUNDLE_URI =
        "chrome://browser/locale/places/places.properties";
      this.__bundle = Cc["@mozilla.org/intl/stringbundle;1"].
                      getService(Ci.nsIStringBundleService).
                      createBundle(PLACES_STRING_BUNDLE_URI);
    }
    return this.__bundle;
  },

  getFormattedString: function PU_getFormattedString(key, params) {
    return this._bundle.formatStringFromName(key, params, params.length);
  },
  
  getString: function PU_getString(key) {
    return this._bundle.GetStringFromName(key);
  },

  /**
   * Determines whether or not a ResultNode is a Bookmark folder or not.
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a Bookmark folder, false otherwise
   */
  nodeIsFolder: function PU_nodeIsFolder(aNode) {
    NS_ASSERT(aNode, "null node");
    return (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER);
  },

  /**
   * Determines whether or not a ResultNode represents a bookmarked URI.
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node represents a bookmarked URI, false otherwise
   */
  nodeIsBookmark: function PU_nodeIsBookmark(aNode) {
    NS_ASSERT(aNode, "null node");
    return aNode.bookmarkId > 0;
  },

  /**
   * Determines whether or not a ResultNode is a Bookmark separator.
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a Bookmark separator, false otherwise
   */
  nodeIsSeparator: function PU_nodeIsSeparator(aNode) {
    NS_ASSERT(aNode, "null node");

    return (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR);
  },

  /**
   * Determines whether or not a ResultNode is a URL item or not
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a URL item, false otherwise
   */
  nodeIsURI: function PU_nodeIsURI(aNode) {
    NS_ASSERT(aNode, "null node");

    const NHRN = Ci.nsINavHistoryResultNode;
    return aNode.type == NHRN.RESULT_TYPE_URI ||
           aNode.type == NHRN.RESULT_TYPE_VISIT ||
           aNode.type == NHRN.RESULT_TYPE_FULL_VISIT;
  },

  /**
   * Determines whether or not a ResultNode is a Query item or not
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a Query item, false otherwise
   */
  nodeIsQuery: function PU_nodeIsQuery(aNode) {
    NS_ASSERT(aNode, "null node");

    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
  },

  /**
   * Determines if a node is read only (children cannot be inserted, sometimes
   * they cannot be removed depending on the circumstance)
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is readonly, false otherwise
   */
  nodeIsReadOnly: function PU_nodeIsReadOnly(aNode) {
    NS_ASSERT(aNode, "null node");

    if (this.nodeIsFolder(aNode))
      return this.bookmarks.getFolderReadonly(asFolder(aNode).folderId);
    if (this.nodeIsQuery(aNode))
      return asQuery(aNode).childrenReadOnly;
    return false;
  },

  /**
   * Determines whether or not a ResultNode is a host folder or not
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a host item, false otherwise
   */
  nodeIsHost: function PU_nodeIsHost(aNode) {
    NS_ASSERT(aNode, "null node");

    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_HOST;
  },

  /**
   * Determines whether or not a ResultNode is a container item or not
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a container item, false otherwise
   */
  nodeIsContainer: function PU_nodeIsContainer(aNode) {
    NS_ASSERT(aNode, "null node");

    const NHRN = Ci.nsINavHistoryResultNode;
    return aNode.type == NHRN.RESULT_TYPE_HOST ||
           aNode.type == NHRN.RESULT_TYPE_QUERY ||
           aNode.type == NHRN.RESULT_TYPE_FOLDER ||
           aNode.type == NHRN.RESULT_TYPE_REMOTE_CONTAINER;
  },

  /**
   * Determines whether or not a ResultNode is a remotecontainer item.
   * ResultNote may be either a remote container result type or a bookmark folder
   * with a nonempty remoteContainerType.  The remote container result node
   * type is for dynamically created remote containers (i.e., for the file
   * browser service where you get your folders in bookmark menus).  Bookmark
   * folders are marked as remote containers when some other component is
   * registered as interested in them and providing some operations, in which
   * case their remoteContainerType indicates which component is thus registered.
   * For exmaple, the livemark service uses this mechanism.
   * @param   aNode
   *          A NavHistoryResultNode
   * @returns true if the node is a container item, false otherwise
   */
  nodeIsRemoteContainer: function PU_nodeIsRemoteContainer(aNode) {
    NS_ASSERT(aNode, "null node");

    const NHRN = Ci.nsINavHistoryResultNode;
    if (aNode.type == NHRN.RESULT_TYPE_REMOTE_CONTAINER)
      return true;
    if (this.nodeIsFolder(aNode))
      return asContainer(aNode).remoteContainerType != "";
    return false;
  },

 /**
  * Determines whether a ResultNode is a remote container registered by the
  * livemark service.
  * @param aNode
  *        A NavHistory Result Node
  * @returns true if the node is a livemark container item
  */
  nodeIsLivemarkContainer: function PU_nodeIsLivemarkContainer(aNode) {
    return (this.nodeIsRemoteContainer(aNode) &&
            asContainer(aNode).remoteContainerType ==
               "@mozilla.org/browser/livemark-service;2");
  },

 /**
  * Determines whether a ResultNode is a live-bookmark item
  * @param aNode
  *        A NavHistory Result Node
  * @returns true if the node is a livemark container item
  */
  nodeIsLivemarkItem: function PU_nodeIsLivemarkItem(aNode) {
    if (this.nodeIsBookmark(aNode)) {
      var placeURI = this.bookmarks.getItemURI(aNode.bookmarkId);
      if (this.annotations.hasAnnotation(placeURI, "livemark/bookmarkFeedURI"))
        return true;
    }

    return false;
  },

  /**
   * Determines whether or not a node is a readonly folder. 
   * @param   aNode
   *          The node to test.
   * @returns true if the node is a readonly folder.
  */
  isReadonlyFolder: function(aNode) {
    NS_ASSERT(aNode, "null node");

    return this.nodeIsFolder(aNode) &&
           this.bookmarks.getFolderReadonly(asFolder(aNode).folderId);
  },

  /**
   * Gets the index of a node within its parent container
   * @param   aNode
   *          The node to look up
   * @returns The index of the node within its parent container, or -1 if the
   *          node was not found or the node specified has no parent.
   */
  getIndexOfNode: function PU_getIndexOfNode(aNode) {
    NS_ASSERT(aNode, "null node");

    var parent = aNode.parent;
    if (!parent || !PlacesUtils.nodeIsContainer(parent))
      return -1;
    var wasOpen = parent.containerOpen;
    parent.containerOpen = true;
    var cc = parent.childCount;
    for (var i = 0; i < cc && asContainer(parent).getChild(i) != aNode; ++i);
    parent.containerOpen = wasOpen;
    return i < cc ? i : -1;
  },

  /**
   * String-wraps a NavHistoryResultNode according to the rules of the specified
   * content type.
   * @param   aNode
   *          The Result node to wrap (serialize)
   * @param   aType
   *          The content type to serialize as
   * @param   [optional] aOverrideURI
   *          Used instead of the node's URI if provided.
   *          This is useful for wrapping a container as TYPE_X_MOZ_URL,
   *          TYPE_HTML or TYPE_UNICODE.
   * @returns A string serialization of the node
   */
  wrapNode: function PU_wrapNode(aNode, aType, aOverrideURI) {
    switch (aType) {
    case this.TYPE_X_MOZ_PLACE_CONTAINER:
    case this.TYPE_X_MOZ_PLACE:
    case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      // Data is encoded like this:
      // bookmarks folder: <folderId>\n<>\n<parentId>\n<indexInParent>
      // uri:              0\n<uri>\n<parentId>\n<indexInParent>
      // bookmark:         <bookmarkId>\n<uri>\n<parentId>\n<indexInParent>
      // separator:        0\n<>\n<parentId>\n<indexInParent>
      var wrapped = "";
      if (this.nodeIsFolder(aNode))
        wrapped += asFolder(aNode).folderId + NEWLINE;
      else if (this.nodeIsBookmark(aNode))
        wrapped += aNode.bookmarkId + NEWLINE;
      else
        wrapped += "0" + NEWLINE;

      if (this.nodeIsURI(aNode) || this.nodeIsQuery(aNode))
        wrapped += aNode.uri + NEWLINE;
      else
        wrapped += NEWLINE;

      if (this.nodeIsFolder(aNode.parent))
        wrapped += asFolder(aNode.parent).folderId + NEWLINE;
      else
        wrapped += "0" + NEWLINE;

      wrapped += this.getIndexOfNode(aNode);
      return wrapped;
    case this.TYPE_X_MOZ_URL:
      return (aOverrideURI || aNode.uri) + NEWLINE + aNode.title;
    case this.TYPE_HTML:
      return "<A HREF=\"" + (aOverrideURI || aNode.uri) + "\">" +
             aNode.title + "</A>";
    }
    // case this.TYPE_UNICODE:
    return (aOverrideURI || aNode.uri);
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
  _getURIItemCopyTransaction: function (aURI, aContainer, aIndex) {
    var itemTitle = this.history.getPageTitle(aURI);
    var createTxn = new PlacesCreateItemTransaction(aURI, aContainer, aIndex);
    createTxn.childTransactions.push(
      new PlacesEditItemTitleTransaction(-1, itemTitle));
    return new PlacesAggregateTransaction("ItemCopy", [createTxn]);
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
   * @returns A nsITransaction object that performs the copy.
   */
  _getBookmarkItemCopyTransaction: function (aID, aContainer, aIndex) {
    var itemURL = this.bookmarks.getBookmarkURI(aID);
    var itemTitle = this.bookmarks.getItemTitle(aID);
    var createTxn = new PlacesCreateItemTransaction(itemURL, aContainer, aIndex);
    createTxn.childTransactions.push(
      new PlacesEditItemTitleTransaction(-1, itemTitle));
    return new PlacesAggregateTransaction("ItemCopy", [createTxn]);
  },

  /**
   * Gets a transaction for copying (recursively nesting to include children)
   * a folder and its contents from one folder to another.
   * @param   aData
   *          Unwrapped dropped folder data
   * @param   aContainer
   *          The container we are copying into
   * @param   aIndex
   *          The index in the destination container to insert the new items
   * @returns A nsITransaction object that will perform the copy.
   */
  _getFolderCopyTransaction:
  function PU__getFolderCopyTransaction(aData, aContainer, aIndex) {
    var self = this;
    function getChildTransactions(folderId) {
      var childTransactions = [];
      var children = self.getFolderContents(folderId, false, false);
      var cc = children.childCount;
      var txn = null;
      for (var i = 0; i < cc; ++i) {
        var node = children.getChild(i);
        if (self.nodeIsFolder(node)) {
          var nodeFolderId = asFolder(node).folderId;
          var title = self.bookmarks.getFolderTitle(nodeFolderId);
          txn = new PlacesCreateFolderTransaction(title, -1, aIndex);
          txn.childTransactions = getChildTransactions(nodeFolderId);
        }
        else if (self.nodeIsBookmark(node)) {
          txn = self._getBookmarkItemCopyTransaction(self._uri(node.uri), -1,
                                                     aIndex);
        }
        else if (self.nodeIsURI(node) || self.nodeIsQuery(node)) {
          txn = self._getURIItemCopyTransaction(self._uri(node.uri), -1,
                                                aIndex);
        }
        else if (self.nodeIsSeparator(node)) {
          txn = new PlacesCreateSeparatorTransaction(-1, aIndex);
        }
        childTransactions.push(txn);
      }
      return childTransactions;
    }

    var title = this.bookmarks.getFolderTitle(aData.id);
    var createTxn =
      new PlacesCreateFolderTransaction(title, aContainer, aIndex);
    createTxn.childTransactions =
      getChildTransactions(aData.id, createTxn);
    return createTxn;
  },

  /**
   * Unwraps data from the Clipboard or the current Drag Session.
   * @param   blob
   *          A blob (string) of data, in some format we potentially know how
   *          to parse.
   * @param   type
   *          The content type of the blob.
   * @returns An array of objects representing each item contained by the source.
   */
  unwrapNodes: function PU_unwrapNodes(blob, type) {
    // We use \n here because the transferable system converts \r\n to \n
    var parts = blob.split("\n");
    var nodes = [];
    for (var i = 0; i < parts.length; ++i) {
      var data = { };
      switch (type) {
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
        // Data in these types has 4 parts, so if there are less than 4 parts
        // remaining, the data blob is malformed and we should stop.
        if (i > (parts.length - 4))
          break;
        nodes.push({  id: parseInt(parts[i++]),
                      uri: parts[i] ? this._uri(parts[i]) : null,
                      parent: parseInt(parts[++i]),
                      index: parseInt(parts[++i]) });
        break;
      case this.TYPE_X_MOZ_URL:
        // See above.
        if (i > (parts.length - 2))
          break;
        nodes.push({  uri: this._uri(parts[i++]),
                      title: parts[i] });
        break;
      case this.TYPE_UNICODE:
        // See above.
        if (i > (parts.length - 1))
          break;
        nodes.push({  uri: this._uri(parts[i]) });
        break;
      default:
        LOG("Cannot unwrap data of type " + type);
        throw Cr.NS_ERROR_INVALID_ARG;
      }
    }
    return nodes;
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
    switch (type) {
    case this.TYPE_X_MOZ_PLACE_CONTAINER:
      if (data.id > 0 && data.uri == null) {
        // Place is a folder.
        if (copy)
          return this._getFolderCopyTransaction(data, container, index);
        return new PlacesMoveFolderTransaction(data.id, data.parent,
                                               data.index, container,
                                               index);
      }
    case this.TYPE_X_MOZ_PLACE:
      if (data.id > 0) {
        if (copy)
          return this._getBookmarkItemCopyTransaction(data.id, container, index);

        return new PlacesMoveItemTransaction(data.id, data.uri, data.parent,
                                             data.index, container,
                                             index);
      }
      return this._getURIItemCopyTransaction(data.uri, container, index);
    case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      if (copy) {
        // There is no data in a separator, so copying it just amounts to
        // inserting a new separator.
        return new PlacesCreateSeparatorTransaction(container, index);
      }
      // Similarly, moving a separator is just removing the old one and
      // then creating a new one.
      var removeTxn =
        new PlacesRemoveSeparatorTransaction(data.parent, data.index);
      var createTxn =
        new PlacesCreateSeparatorTransaction(container, index);
      return new PlacesAggregateTransaction("SeparatorMove", [removeTxn, createTxn]);
    case this.TYPE_X_MOZ_URL:
    case this.TYPE_UNICODE:
      // Creating and Setting the title is a two step process
      var createTxn =
        new PlacesCreateItemTransaction(data.uri, container, index);
      var title = type == TYPE_X_MOZ_URL ? data.title : data.uri;
      createTxn.childTransactions.push(
          new PlacesEditItemTitleTransaction(-1, title));
      return createTxn;
    }
    return null;
  },

  /**
   * Generates a HistoryResultNode for the contents of a folder.
   * @param   folderId
   *          The folder to open
   * @param   [optional] excludeItems
   *          True to hide all items (individual bookmarks). This is used on
   *          the left places pane so you just get a folder hierarchy.
   * @param   [optional] expandQueries
   *          True to make query items expand as new containers. For managing,
   *          you want this to be false, for menus and such, you want this to
   *          be true.
   * @returns A HistoryContainerResultNode containing the contents of the
   *          folder. This container is guaranteed to be open.
   */
  getFolderContents:
  function PU_getFolderContents(aFolderId, aExcludeItems, aExpandQueries) {
    var query = this.history.getNewQuery();
    query.setFolders([aFolderId], 1);
    var options = this.history.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeItems = aExcludeItems;
    options.expandQueries = aExpandQueries;

    var result = this.history.executeQuery(query, options);
    result.root.containerOpen = true;
    return asContainer(result.root);
  },

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
   * @return true if any transaction has been performed.
   *
   * Notes:
   *  - the location, description, keyword and "load in sidebar" fields are
   *    visible only if there is no initial URI (aURI is null).
   *  - When aDefaultInsertionPoint is not set, the dialog defaults to the
   *    bookmarks root folder.
   */
  showAddBookmarkUI: function PU_showAddBookmarkUI(aURI,
                                                   aTitle,
                                                   aDescription,
                                                   aDefaultInsertionPoint,
                                                   aShowPicker,
                                                   aLoadInSidebar) {
    var info = {
      action: "add",
      type: "bookmark",
      hiddenRows: []
    };

    if (aURI) {
      info.uri = aURI;
      info.hiddenRows = ["location", "keyword", "description",
                         "load in sidebar"];
    }

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

    return this._showBookmarkDialog(info);
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
      type: "livemark",
      hiddenRows: []
    };

    if (aFeedURI) {
      info.feedURI = aFeedURI;
      info.hiddenRows = ["description"];
    }
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
    return this._showBookmarkDialog(info);
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
  showAddMultiBookmarkUI: function PU_showAddMultiBookmarkUI(aURIList) {
    NS_ASSERT(aURIList.length,
              "showAddMultiBookmarkUI expects a list of nsIURI objects");
    var info = {
      action: "add",
      type: "folder with items",
      hiddenRows: ["description"],
      URIList: aURIList
    };
    return this._showBookmarkDialog(info);
  },

  /**
   * Opens the bookmark properties panel for a given bookmark idnetifier.
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
   * @return true if any transaction has been performed.
   */
  _showBookmarkDialog: function PU__showBookmarkDialog(aInfo) {
    window.openDialog("chrome://browser/content/places/bookmarkProperties.xul",
                      "", "width=600,height=400,chrome,dependent,modal,resizable",
                      aInfo);
    return ("performed" in aInfo && aInfo.performed);
  },

  /**
   * Returns the closet ancestor places view for the given DOM node
   * @param aNode
   *        a DOM node
   * @return the closet ancestor places view if exists, null otherwsie.
   */
  getViewForNode: function(aNode) {
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
   * Allows opening of javascript/data URI only if the given node is
   * bookmarked (see bug 224521).
   * @param aURINode
   *        a URI node
   * @return true if it's safe to open the node in the browser, false otherwise.
   * 
   */
  checkURLSecurity: function PU_checkURLSecurity(aURINode) {
    if (!this.nodeIsBookmark(aURINode)) {
      var uri = this._uri(aURINode.uri);
      if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
        const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
        var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService).
                             createBundle(BRANDING_BUNDLE_URI).
                             GetStringFromName("brandShortName");
        var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                            getService(Ci.nsIPromptService);

        var errorStr = this.getString("load-js-data-url-error");
        promptService.alert(window, brandStr, errorStr);
        return false;
      }
    }
    return true;
  },

  /**
   * Fetch all annotations for a URI, including all properties of each
   * annotation which would be required to recreate it.
   * @param aURI
   *        The URI for which annotations are to be retrieved.
   * @return Array of objects, each containing the following properties:
   *         name, flags, expires, mimeType, type, value
   */
  getAnnotationsForURI: function PU_getAnnotationsForURI(aURI) {
    var annosvc = this.annotations;
    var sv = Ci.mozIStorageValueArray;
    var annos = [], val = null;
    var annoNames = annosvc.getPageAnnotationNames(aURI, {});
    for (var i = 0; i < annoNames.length; i++) {
      var flags = {}, exp = {}, mimeType = {}, storageType = {};
      annosvc.getAnnotationInfo(aURI, annoNames[i], flags, exp, mimeType, storageType);
      switch (storageType.value) {
        case sv.VALUE_TYPE_INTEGER:
          val = annosvc.getAnnotationInt64(aURI, annoNames[i]);
          break;
        case sv.VALUE_TYPE_FLOAT:
          val = annosvc.getAnnotationDouble(aURI, annoNames[i]);
          break;
        case sv.VALUE_TYPE_TEXT:
          val = annosvc.getAnnotationString(aURI, annoNames[i]);
          break;
        case sv.VALUE_TYPE_BLOB:
          val = annosvc.getAnnotationBinary(aURI, annoNames[i]);
          break;
      }
      annos.push({name: annoNames[i],
                  flags: flags.value,
                  expires: exp.value,
                  mimeType: mimeType.value,
                  type: storageType.value,
                  value: val});
    }
    return annos;
  },

  /**
   * Annotate a URI with a batch of annotations.
   * @param aURI
   *        The URI for which annotations are to be retrieved.
   * @param aAnnotations
   *        Array of objects, each containing the following properties:
   *        name, flags, expires, type, mimeType (only used for binary
   *        annotations) value.
   */
  setAnnotationsForURI: function PU_setAnnotationsForURI(aURI, aAnnos) {
    var annosvc = this.annotations;
    var sv = Ci["mozIStorageValueArray"];
    aAnnos.forEach(function(anno) {
      switch (anno.type) {
        case sv.VALUE_TYPE_INTEGER:
          annosvc.setAnnotationInt64(aURI, anno.name, anno.value,
                                                     anno.flags, anno.expires);
          break;
        case sv.VALUE_TYPE_FLOAT:
          annosvc.setAnnotationDouble(aURI, anno.name, anno.value,
                                                      anno.flags, anno.expires);
          break;
        case sv.VALUE_TYPE_TEXT:
          annosvc.setAnnotationString(aURI, anno.name, anno.value,
                                                      anno.flags, anno.expires);
          break;
        case sv.VALUE_TYPE_BLOB:
          annosvc.setAnnotationBinary(aURI, anno.name, anno.value,
                                                      anno.value.length, anno.mimeType,
                                                      anno.flags, anno.expires);
          break;
      }
    });
  },

  /**
   * Helper for getting a serialized Places query for a particular folder.
   * @param aFolderId The folder id to get a query for.
   * @return string serialized place URI
   */
  getQueryStringForFolder: function PU_getQueryStringForFolder(aFolderId) {
    var options = this.history.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var query = this.history.getNewQuery();
    query.setFolders([aFolderId], 1);
    return this.history.queriesToQueryString([query], 1, options);
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
      if (metaElements[i].localName.toLowerCase() == "description" || 
          metaElements[i].httpEquiv.toLowerCase() == "description") {
        return metaElements[i].content;
      }
    }
    return "";
  }
};

PlacesUtils.GENERIC_VIEW_DROP_TYPES = [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                       PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
                                       PlacesUtils.TYPE_X_MOZ_PLACE,
                                       PlacesUtils.TYPE_X_MOZ_URL];
