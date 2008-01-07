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

Components.utils.import("resource://gre/modules/JSON.jsm");

const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const DESCRIPTION_ANNO = "bookmarkProperties/description";
const POST_DATA_ANNO = "bookmarkProperties/POSTData";
const LMANNO_FEEDURI = "livemark/feedURI";
const LMANNO_SITEURI = "livemark/siteURI";
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
  get bookmarks() {
    delete this.bookmarks;
    return this.bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                            getService(Ci.nsINavBookmarksService);
  },

  /**
   * The Nav History Service.
   */
  get history() {
    delete this.history;
    return this.history = Cc["@mozilla.org/browser/nav-history-service;1"].
                          getService(Ci.nsINavHistoryService);
  },

  get globalHistory() {
    delete this.globalHistory;
    return this.globalHistory = Cc["@mozilla.org/browser/global-history;2"].
                                getService(Ci.nsIBrowserHistory);
  },

  /**
   * The Live Bookmark Service.
   */
  get livemarks() {
    delete this.livemarks;
    return this.livemarks = Cc["@mozilla.org/browser/livemark-service;2"].
                            getService(Ci.nsILivemarkService);
  },

  /**
   * The Annotations Service.
   */
  get annotations() {
    delete this.annotations;
    return this.annotations = Cc["@mozilla.org/browser/annotation-service;1"].
                              getService(Ci.nsIAnnotationService);
  },

  /**
   * The Favicons Service
   */
  get favicons() {
    delete this.favicons;
    return this.favicons = Cc["@mozilla.org/browser/favicon-service;1"].
                           getService(Ci.nsIFaviconService);
  },

  /**
   * The Microsummary Service
   */
  get microsummaries() {
    delete this.microsummaries;
    return this.microsummaries = Cc["@mozilla.org/microsummary/service;1"].
                                 getService(Ci.nsIMicrosummaryService);
  },

  /**
   * The Places Tagging Service
   */
  get tagging() {
    delete this.tagging;
    return this.tagging = Cc["@mozilla.org/browser/tagging-service;1"].
                          getService(Ci.nsITaggingService);
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
   * Makes a URI from a spec.
   * @param   aSpec
   *          The string spec of the URI
   * @returns A URI object for the spec.
   */
  _uri: function PU__uri(aSpec) {
    NS_ASSERT(aSpec, "empty URL spec");
    return IO.newURI(aSpec);
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
   * Determines whether or not a ResultNode is a Bookmark folder or not.
   * @param   aNode
   *          A result node
   * @returns true if the node is a Bookmark folder, false otherwise
   */
  nodeIsFolder: function PU_nodeIsFolder(aNode) {
    NS_ASSERT(aNode, "null node");
    return (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER);
  },

  /**
   * Determines whether or not a ResultNode represents a bookmarked URI.
   * @param   aNode
   *          A result node
   * @returns true if the node represents a bookmarked URI, false otherwise
   */
  nodeIsBookmark: function PU_nodeIsBookmark(aNode) {
    NS_ASSERT(aNode, "null node");
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_URI &&
           aNode.itemId != -1;
  },

  /**
   * Determines whether or not a ResultNode is a Bookmark separator.
   * @param   aNode
   *          A result node
   * @returns true if the node is a Bookmark separator, false otherwise
   */
  nodeIsSeparator: function PU_nodeIsSeparator(aNode) {
    NS_ASSERT(aNode, "null node");

    return (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR);
  },

  /**
   * Determines whether or not a ResultNode is a visit item or not
   * @param   aNode
   *          A result node
   * @returns true if the node is a visit item, false otherwise
   */
  nodeIsVisit: function PU_nodeIsVisit(aNode) {
    NS_ASSERT(aNode, "null node");

    const NHRN = Ci.nsINavHistoryResultNode;
    var type = aNode.type;
    return type == NHRN.RESULT_TYPE_VISIT ||
           type == NHRN.RESULT_TYPE_FULL_VISIT;
  },

  /**
   * Determines whether or not a ResultNode is a URL item or not
   * @param   aNode
   *          A result node
   * @returns true if the node is a URL item, false otherwise
   */
  uriTypes: [Ci.nsINavHistoryResultNode.RESULT_TYPE_URI,
             Ci.nsINavHistoryResultNode.RESULT_TYPE_VISIT,
             Ci.nsINavHistoryResultNode.RESULT_TYPE_FULL_VISIT],
  nodeIsURI: function PU_nodeIsURI(aNode) {
    NS_ASSERT(aNode, "null node");
    return this.uriTypes.indexOf(aNode.type) != -1;
  },

  /**
   * Determines whether or not a ResultNode is a Query item or not
   * @param   aNode
   *          A result node
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
   *          A result node
   * @returns true if the node is readonly, false otherwise
   */
  nodeIsReadOnly: function PU_nodeIsReadOnly(aNode) {
    NS_ASSERT(aNode, "null node");

    if (this.nodeIsFolder(aNode))
      return this.bookmarks.getFolderReadonly(aNode.itemId);
    if (this.nodeIsQuery(aNode))
      return asQuery(aNode).childrenReadOnly;
    return false;
  },

  /**
   * Determines whether or not a ResultNode is a host folder or not
   * @param   aNode
   *          A result node
   * @returns true if the node is a host item, false otherwise
   */
  nodeIsHost: function PU_nodeIsHost(aNode) {
    NS_ASSERT(aNode, "null node");
    return aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_HOST;
  },

  /**
   * Determines whether or not a ResultNode is a container item or not
   * @param   aNode
   *          A result node
   * @returns true if the node is a container item, false otherwise
   */
  containerTypes: [Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_HOST,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_DAY,
                   Ci.nsINavHistoryResultNode.RESULT_TYPE_DYNAMIC_CONTAINER],
  nodeIsContainer: function PU_nodeIsContainer(aNode) {
    NS_ASSERT(aNode, "null node");
    return this.containerTypes.indexOf(aNode.type) != -1;
  },

  /**
   * Determines whether or not a result-node is a dynamic-container item.
   * The dynamic container result node type is for dynamically created
   * containers (e.g. for the file browser service where you get your folders
   * in bookmark menus).
   * @param   aNode
   *          A result node
   * @returns true if the node is a dynamic container item, false otherwise
   */
  nodeIsDynamicContainer: function PU_nodeIsDynamicContainer(aNode) {
    NS_ASSERT(aNode, "null node");
    if (aNode.type == NHRN.RESULT_TYPE_DYNAMIC_CONTAINER)
      return true;
    return false;
  },

 /**
  * Determines whether a result node is a remote container registered by the
  * livemark service.
  * @param aNode
  *        A result Node
  * @returns true if the node is a livemark container item
  */
  nodeIsLivemarkContainer: function PU_nodeIsLivemarkContainer(aNode) {
    // Use the annotations service directly to avoid instantiating
    // the Livemark service on startup. (bug 398300)
    return this.nodeIsFolder(aNode) &&
           this.annotations.itemHasAnnotation(aNode.itemId, LMANNO_FEEDURI);
  },

 /**
  * Determines whether a result node is a live-bookmark item
  * @param aNode
  *        A result node
  * @returns true if the node is a livemark container item
  */
  nodeIsLivemarkItem: function PU_nodeIsLivemarkItem(aNode) {
    return aNode.parent && this.nodeIsLivemarkContainer(aNode.parent);
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
           this.bookmarks.getFolderReadonly(aNode.itemId);
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
    if (!parent)
      return -1;
    var wasOpen = parent.containerOpen;
    var result, oldViewer;
    if (!wasOpen) {
      result = parent.parentResult;
      oldViewer = result.viewer;
      result.viewer = null;
      parent.containerOpen = true;
    }
    var cc = parent.childCount;
    for (var i = 0; i < cc && parent.getChild(i) != aNode; ++i);
    if (!wasOpen) {
      parent.containerOpen = false;
      result.viewer = oldViewer;
    }
    return i < cc ? i : -1;
  },

  /**
   * String-wraps a result node according to the rules of the specified
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
    var self = this;

    // when wrapping a node, we want all the items, even if the original
    // query options are excluding them.
    // this can happen when copying from the left hand pane of the bookmarks
    // organizer
    function convertNode(cNode) {
      try {
        if (self.nodeIsFolder(cNode) && cNode.queryOptions.excludeItems)
          return self.getFolderContents(cNode.itemId, false, true).root;
      }
      catch (e) {
      }
      return cNode;
    }

    switch (aType) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
        function gatherDataPlace(bNode) {
          var nodeId = 0;
          if (bNode.itemId != -1)
            nodeId = bNode.itemId;
          var nodeUri = bNode.uri
          var nodeTitle = bNode.title;
          var nodeParentId = 0;
          if (bNode.parent && self.nodeIsFolder(bNode.parent))
            nodeParentId = bNode.parent.itemId;
          var nodeIndex = self.getIndexOfNode(bNode);
          var nodeKeyword = self.bookmarks.getKeywordForBookmark(bNode.itemId);
          var nodeAnnos = self.getAnnotationsForItem(bNode.itemId);
          var nodeType = "";
          if (self.nodeIsContainer(bNode))
            nodeType = self.TYPE_X_MOZ_PLACE_CONTAINER;
          else if (self.nodeIsURI(bNode)) // a bookmark or a history visit
            nodeType = self.TYPE_X_MOZ_PLACE;
          else if (self.nodeIsSeparator(bNode))
            nodeType = self.TYPE_X_MOZ_PLACE_SEPARATOR;

          var node = { id: nodeId,
                       uri: nodeUri,
                       title: nodeTitle,
                       parent: nodeParentId,
                       index: nodeIndex,
                       keyword: nodeKeyword,
                       annos: nodeAnnos,
                       type: nodeType };

          // Recurse down children if the node is a folder
          if (self.nodeIsContainer(bNode)) {
            asContainer(bNode);
            if (self.nodeIsLivemarkContainer(bNode)) {
              // just save the livemark info, reinstantiate on other end
              var feedURI = self.livemarks.getFeedURI(bNode.itemId).spec;
              var siteURI = self.livemarks.getSiteURI(bNode.itemId).spec;
              node.uri = { feed: feedURI,
                           site: siteURI };
            }
            else { // bookmark folders + history containers
              var wasOpen = bNode.containerOpen;
              if (!wasOpen)
                bNode.containerOpen = true;
              var childNodes = [];
              var cc = bNode.childCount;
              for (var i = 0; i < cc; ++i) {
                var childObj = gatherDataPlace(bNode.getChild(i));
                if (childObj != null)
                  childNodes.push(childObj);
              }
              var parent = node;
              node = { folder: parent,
                       children: childNodes,
                       type: self.TYPE_X_MOZ_PLACE_CONTAINER };
              bNode.containerOpen = wasOpen;
            }
          }
          return node;
        }
        return JSON.toString(gatherDataPlace(convertNode(aNode)));

      case this.TYPE_X_MOZ_URL:
        function gatherDataUrl(bNode) {
          if (self.nodeIsLivemarkContainer(bNode)) {
            var siteURI = self.livemarks.getSiteURI(bNode.itemId).spec;
            return siteURI + NEWLINE + bNode.title;
          }
          if (self.nodeIsURI(bNode))
            return (aOverrideURI || bNode.uri) + NEWLINE + bNode.title;
          // ignore containers and separators - items without valid URIs
          return "";
        }
        return gatherDataUrl(convertNode(aNode));

      case this.TYPE_HTML:
        function gatherDataHtml(bNode) {
          function htmlEscape(s) {
            s = s.replace(/&/g, "&amp;");
            s = s.replace(/>/g, "&gt;");
            s = s.replace(/</g, "&lt;");
            s = s.replace(/"/g, "&quot;");
            s = s.replace(/'/g, "&apos;");
            return s;
          }
          // escape out potential HTML in the title
          var escapedTitle = htmlEscape(bNode.title);
          if (self.nodeIsLivemarkContainer(bNode)) {
            var siteURI = self.livemarks.getSiteURI(bNode.itemId).spec;
            return "<A HREF=\"" + siteURI + "\">" + escapedTitle + "</A>" + NEWLINE;
          }
          if (self.nodeIsContainer(bNode)) {
            asContainer(bNode);
            var wasOpen = bNode.containerOpen;
            if (!wasOpen)
              bNode.containerOpen = true;

            var childString = "<DL><DT>" + escapedTitle + "</DT>" + NEWLINE;
            var cc = bNode.childCount;
            for (var i = 0; i < cc; ++i)
              childString += "<DD>"
                             + NEWLINE
                             + gatherDataHtml(bNode.getChild(i))
                             + "</DD>"
                             + NEWLINE;
            bNode.containerOpen = wasOpen;
            return childString + "</DL>" + NEWLINE;
          }
          if (self.nodeIsURI(bNode))
            return "<A HREF=\"" + bNode.uri + "\">" + escapedTitle + "</A>" + NEWLINE;
          if (self.nodeIsSeparator(bNode))
            return "<HR>" + NEWLINE;
          return "";
        }
        return gatherDataHtml(convertNode(aNode));
    }
    // case this.TYPE_UNICODE:
    function gatherDataText(bNode) {
      if (self.nodeIsLivemarkContainer(bNode))
        return self.livemarks.getSiteURI(bNode.itemId).spec;
      if (self.nodeIsContainer(bNode)) {
        asContainer(bNode);
        var wasOpen = bNode.containerOpen;
        if (!wasOpen)
          bNode.containerOpen = true;

        var childString = bNode.title + NEWLINE;
        var cc = bNode.childCount;
        for (var i = 0; i < cc; ++i) {
          var child = bNode.getChild(i);
          var suffix = i < (cc - 1) ? NEWLINE : "";
          childString += gatherDataText(child) + suffix;
        }
        bNode.containerOpen = wasOpen;
        return childString;
      }
      if (self.nodeIsURI(bNode))
        return (aOverrideURI || bNode.uri);
      if (self.nodeIsSeparator(bNode))
        return "--------------------";
      return "";
    }

    return gatherDataText(convertNode(aNode));
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
    return this.ptm.createItem(IO.newURI(aData.uri), aContainer, aIndex,
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
    var itemURL = IO.newURI(aData.uri);
    var itemTitle = aData.title;
    var keyword = aData.keyword;
    var annos = aData.annos;
    if (aExcludeAnnotations) {
      annos =
        annos.filter(function(aValue, aIndex, aArray) {
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

        if (node.type == self.TYPE_X_MOZ_PLACE_CONTAINER) {
          if (node.folder) {
            var title = node.folder.title;
            var annos = node.folder.annos;
            var folderItemsTransactions =
              getChildItemsTransactions(node.children);
            txn = self.ptm.createFolder(title, -1, index, annos,
                                        folderItemsTransactions);
          }
          else { // node is a livemark
            var feedURI = IO.newURI(node.uri.feed);
            var siteURI = IO.newURI(node.uri.site);
            txn = self.ptm.createLivemark(feedURI, siteURI, node.title,
                                          aContainer, index, node.annos);
          }
        }
        else if (node.type == self.TYPE_X_MOZ_PLACE_SEPARATOR)
          txn = self.ptm.createSeparator(-1, index);
        else if (node.type == self.TYPE_X_MOZ_PLACE)
          txn = self._getBookmarkItemCopyTransaction(node, -1, index);

        NS_ASSERT(txn, "Unexpected item under a bookmarks folder");
        if (txn)
          childItemsTransactions.push(txn);
      }
      return childItemsTransactions;
    }

    var title = aData.folder.title;
    var annos = aData.folder.annos;
    var childItems = getChildItemsTransactions(aData.children);
    if (aData.folder.dateAdded)
      childItems.push(this.ptm.editItemDateAdded(null, aData.folder.dateAdded));
    if (aData.folder.lastModified)
      childItems.push(this.ptm.editItemLastModified(null, aData.folder.lastModified));
    return this.ptm.createFolder(title, aContainer, aIndex, annos, childItems);
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
    // We split on "\n"  because the transferable system converts "\r\n" to "\n"
    var nodes = [];
    switch(type) {
      case this.TYPE_X_MOZ_PLACE:
      case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      case this.TYPE_X_MOZ_PLACE_CONTAINER:
        nodes = JSON.fromString("[" + blob + "]");
        break;
      case this.TYPE_X_MOZ_URL:
        var parts = blob.split("\n");
        // data in this type has 2 parts per entry, so if there are fewer
        // than 2 parts left, the blob is malformed and we should stop
        if (parts.length % 2)
          break;
        for (var i = 0; i < parts.length; i=i+2) {
          var uriString = parts[i];
          var titleString = parts[i+1];
          // note:  IO.newURI() will throw if uriString is not a valid URI
          if (IO.newURI(uriString)) {
            nodes.push({ uri: uriString,
                         title: titleString ? titleString : uriString });
          }
        }
        break;
      case this.TYPE_UNICODE:
        var parts = blob.split("\n");
        for (var i = 0; i < parts.length; i++) {
          var uriString = parts[i];
          // note: IO.newURI() will throw if uriString is not a valid URI
          if (uriString != "" && IO.newURI(uriString))
            nodes.push({ uri: uriString, title: uriString });
        }
        break;
      default:
        LOG("Cannot unwrap data of type " + type);
        throw Cr.NS_ERROR_INVALID_ARG;
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
    switch (data.type) {
    case this.TYPE_X_MOZ_PLACE_CONTAINER:
      if (data.folder) {
        // Place is a folder.
        if (copy)
          return this._getFolderCopyTransaction(data, container, index);
      }
      else if (copy) {
        // Place is a Livemark Container, should be reinstantiated
        var feedURI = IO.newURI(data.uri.feed);
        var siteURI = IO.newURI(data.uri.site);
        return this.ptm.createLivemark(feedURI, siteURI, data.title, container,
                                       index, data.annos);
      }
      break;
    case this.TYPE_X_MOZ_PLACE:
      if (data.id <= 0)
        return this._getURIItemCopyTransaction(data, container, index);

      if (copy) {
        // Copying a child of a live-bookmark by itself should result
        // as a new normal bookmark item (bug 376731)
        var copyBookmarkAnno =
          this._getBookmarkItemCopyTransaction(data, container, index,
                                               ["livemark/bookmarkFeedURI"]);
        return copyBookmarkAnno;
      }
      break;
    case this.TYPE_X_MOZ_PLACE_SEPARATOR:
      if (copy) {
        // There is no data in a separator, so copying it just amounts to
        // inserting a new separator.
        return this.ptm.createSeparator(container, index);
      }
      break;
    default:
      if (type == this.TYPE_X_MOZ_URL || type == this.TYPE_UNICODE) {
        var title = (type == this.TYPE_X_MOZ_URL) ? data.title : data.uri;
        return this.ptm.createItem(IO.newURI(data.uri), container, index,
                                   title);
      }
      return null;
    }
    if (data.id <= 0)
      return null;

    // Move the item otherwise
    var id = data.folder ? data.folder.id : data.id;
    return this.ptm.moveItem(id, container, index);
  },

  /**
   * Generates a nsINavHistoryResult for the contents of a folder.
   * @param   folderId
   *          The folder to open
   * @param   [optional] excludeItems
   *          True to hide all items (individual bookmarks). This is used on
   *          the left places pane so you just get a folder hierarchy.
   * @param   [optional] expandQueries
   *          True to make query items expand as new containers. For managing,
   *          you want this to be false, for menus and such, you want this to
   *          be true.
   * @returns A nsINavHistoryResult containing the contents of the
   *          folder. The result.root is guaranteed to be open.
   */
  getFolderContents:
  function PU_getFolderContents(aFolderId, aExcludeItems, aExpandQueries) {
    var query = this.history.getNewQuery();
    query.setFolders([aFolderId], 1);
    var options = this.history.getNewQueryOptions();
    options.excludeItems = aExcludeItems;
    options.expandQueries = aExpandQueries;

    var result = this.history.executeQuery(query, options);
    result.root.containerOpen = true;
    return result;
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
    this.globalHistory.markPageAsTyped(this.createFixedURI(aURL));
  },

  /**
   * By calling this before we visit a URL, we will use TRANSITION_BOOKMARK
   * as the transition for the visit to that URL (if we don't have a referrer).
   * This is used when visiting pages from the bookmarks menu, 
   * personal toolbar, and bookmarks from within the places organizer.
   * If we don't call this, we'll treat those visits as TRANSITION_LINK.
   */
  markPageAsFollowedBookmark: function PU_markPageAsFollowedBookmark(aURL) {
    this.history.markPageAsFollowedBookmark(this.createFixedURI(aURL));
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
      var uri = IO.newURI(aURINode.uri);
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
   * Fetch all annotations for a URI, including all properties of each
   * annotation which would be required to recreate it.
   * @param aURI
   *        The URI for which annotations are to be retrieved.
   * @return Array of objects, each containing the following properties:
   *         name, flags, expires, mimeType, type, value
   */
  getAnnotationsForURI: function PU_getAnnotationsForURI(aURI) {
    var annosvc = this.annotations;
    var annos = [], val = null;
    var annoNames = annosvc.getPageAnnotationNames(aURI, {});
    for (var i = 0; i < annoNames.length; i++) {
      var flags = {}, exp = {}, mimeType = {}, storageType = {};
      annosvc.getPageAnnotationInfo(aURI, annoNames[i], flags, exp, mimeType, storageType);
      if (storageType.value == annosvc.TYPE_BINARY) {
        var data = {}, length = {}, mimeType = {};
        annosvc.getPageAnnotationBinary(aURI, annoNames[i], data, length, mimeType);
        val = data.value;
      }
      else
        val = annosvc.getPageAnnotation(aURI, annoNames[i]);

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
   * Fetch all annotations for an item, including all properties of each
   * annotation which would be required to recreate it.
   * @param aItemId
   *        The identifier of the itme for which annotations are to be
   *        retrieved.
   * @return Array of objects, each containing the following properties:
   *         name, flags, expires, mimeType, type, value
   */
  getAnnotationsForItem: function PU_getAnnotationsForItem(aItemId) {
    var annosvc = this.annotations;
    var annos = [], val = null;
    var annoNames = annosvc.getItemAnnotationNames(aItemId, {});
    for (var i = 0; i < annoNames.length; i++) {
      var flags = {}, exp = {}, mimeType = {}, storageType = {};
      annosvc.getItemAnnotationInfo(aItemId, annoNames[i], flags, exp, mimeType, storageType);
      if (storageType.value == annosvc.TYPE_BINARY) {
        var data = {}, length = {}, mimeType = {};
        annosvc.geItemAnnotationBinary(aItemId, annoNames[i], data, length, mimeType);
        val = data.value;
      }
      else
        val = annosvc.getItemAnnotation(aItemId, annoNames[i]);

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
   *        The URI for which annotations are to be set.
   * @param aAnnotations
   *        Array of objects, each containing the following properties:
   *        name, flags, expires, type, mimeType (only used for binary
   *        annotations) value.
   */
  setAnnotationsForURI: function PU_setAnnotationsForURI(aURI, aAnnos) {
    var annosvc = this.annotations;
    aAnnos.forEach(function(anno) {
      var flags = ("flags" in anno) ? anno.flags : 0;
      var expires = ("expires" in anno) ?
        anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
      if (anno.type == annosvc.TYPE_BINARY) {
        annosvc.setPageAnnotationBinary(aURI, anno.name, anno.value,
                                        anno.value.length, anno.mimeType,
                                        flags, expires);
      }
      else
        annosvc.setPageAnnotation(aURI, anno.name, anno.value, flags, expires);
    });
  },

  /**
   * Annotate an item with a batch of annotations.
   * @param aItemId
   *        The identifier of the item for which annotations are to be set
   * @param aAnnotations
   *        Array of objects, each containing the following properties:
   *        name, flags, expires, type, mimeType (only used for binary
   *        annotations) value.
   */
  setAnnotationsForItem: function PU_setAnnotationsForItem(aItemId, aAnnos) {
    var annosvc = this.annotations;
    aAnnos.forEach(function(anno) {
      var flags = ("flags" in anno) ? anno.flags : 0;
      var expires = ("expires" in anno) ?
        anno.expires : Ci.nsIAnnotationService.EXPIRE_NEVER;
      if (anno.type == annosvc.TYPE_BINARY) {
        annosvc.setItemAnnotationBinary(aItemId, anno.name, anno.value,
                                        anno.value.length, anno.mimeType,
                                        flags, expires);
      }
      else {
        annosvc.setItemAnnotation(aItemId, anno.name, anno.value, flags,
                                  expires);
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
      if (metaElements[i].name.toLowerCase() == "description" ||
          metaElements[i].httpEquiv.toLowerCase() == "description") {
        return metaElements[i].content;
      }
    }
    return "";
  },

  // identifier getters for special folders
  get placesRootId() {
    delete this.placesRootId;
    return this.placesRootId = this.bookmarks.placesRoot;
  },

  get bookmarksMenuFolderId() {
    delete this.bookmarksMenuFolderId;
    return this.bookmarksMenuFolderId = this.bookmarks.bookmarksMenuFolder;
  },

  get toolbarFolderId() {
    delete this.toolbarFolderId;
    return this.toolbarFolderId = this.bookmarks.toolbarFolder;
  },

  get tagsFolderId() {
    delete this.tagsFolderId;
    return this.tagsFolderId = this.bookmarks.tagsFolder;
  },

  get unfiledBookmarksFolderId() {
    delete this.unfiledBookmarksFolderId;
    return this.unfiledBookmarksFolderId = this.bookmarks.unfiledBookmarksFolder;
  },

  /**
   * Set the POST data associated with a bookmark, if any.
   * Used by POST keywords.
   *   @param aBookmarkId
   *   @returns string of POST data
   */
  setPostDataForBookmark: function PU_setPostDataForBookmark(aBookmarkId, aPostData) {
    const annos = this.annotations;
    if (aPostData)
      annos.setItemAnnotation(aBookmarkId, POST_DATA_ANNO, aPostData, 
                              0, Ci.nsIAnnotationService.EXPIRE_NEVER);
    else if (annos.itemHasAnnotation(aBookmarkId, POST_DATA_ANNO))
      annos.removeItemAnnotation(aBookmarkId, POST_DATA_ANNO);
  },

  /**
   * Get the POST data associated with a bookmark, if any.
   * @param aBookmarkId
   * @returns string of POST data if set for aBookmarkId. null otherwise.
   */
  getPostDataForBookmark: function PU_getPostDataForBookmark(aBookmarkId) {
    const annos = this.annotations;
    if (annos.itemHasAnnotation(aBookmarkId, POST_DATA_ANNO))
      return annos.getItemAnnotation(aBookmarkId, POST_DATA_ANNO);

    return null;
  },

  /**
   * Get the URI (and any associated POST data) for a given keyword.
   * @param aKeyword string keyword
   * @returns an array containing a string URL and a string of POST data
   */
  getURLAndPostDataForKeyword: function PU_getURLAndPostDataForKeyword(aKeyword) {
    var url = null, postdata = null;
    try {
      var uri = this.bookmarks.getURIForKeyword(aKeyword);
      if (uri) {
        url = uri.spec;
        var bookmarks = this.bookmarks.getBookmarkIdsForURI(uri, {});
        for (let i = 0; i < bookmarks.length; i++) {
          var bookmark = bookmarks[i];
          var kw = this.bookmarks.getKeywordForBookmark(bookmark);
          if (kw == aKeyword) {
            postdata = this.getPostDataForBookmark(bookmark);
            break;
          }
        }
      }
    } catch(ex) {}
    return [url, postdata];
  },

  /**
   * Retrieve the description of an item
   * @param aItemId
   *        item identifier
   * @returns the description of the given item, or an empty string if it is
   * not set.
   */
  getItemDescription: function PU_getItemDescription(aItemId) {
    if (this.annotations.itemHasAnnotation(aItemId, DESCRIPTION_ANNO))
      return this.annotations.getItemAnnotation(aItemId, DESCRIPTION_ANNO);
    return "";
  },

  /**
   * Get the most recently added/modified bookmark for a URL, excluding items
   * under tag or livemark containers. -1 is returned if no item is found.
   */
  getMostRecentBookmarkForURI:
  function PU_getMostRecentBookmarkForURI(aURI) {
    var bmkIds = this.bookmarks.getBookmarkIdsForURI(aURI, {});
    for each (var bk in bmkIds) {
      // Find the first folder which isn't a tag container
      var parent = this.bookmarks.getFolderIdForItem(bk);
      if (parent == this.unfiledBookmarksFolderId)
        return bk;

      var grandparent = this.bookmarks.getFolderIdForItem(parent);
      if (grandparent != this.tagsFolderId &&
          !this.annotations.itemHasAnnotation(parent, LMANNO_FEEDURI))
        return bk;
    }
    return -1;
  },

  getMostRecentFolderForFeedURI:
  function PU_getMostRecentFolderForFeedURI(aURI) {
    var feedSpec = aURI.spec
    var annosvc = this.annotations;
    var livemarks = annosvc.getItemsWithAnnotation(LMANNO_FEEDURI, {});
    for (var i = 0; i < livemarks.length; i++) {
      if (annosvc.getItemAnnotation(livemarks[i], LMANNO_FEEDURI) == feedSpec)
        return livemarks[i];
    }
    return -1;
  },

  getURLsForContainerNode: function PU_getURLsForContainerNode(aNode) {
    let urls = [];
    if (this.nodeIsFolder(aNode) && asQuery(aNode).queryOptions.excludeItems) {
      // grab manually
      let contents = this.getFolderContents(aNode.itemId, false, false).root;
      for (let i = 0; i < contents.childCount; ++i) {
        let child = contents.getChild(i);
        if (this.nodeIsURI(child))
          urls.push({uri: child.uri, isBookmark: this.nodeIsBookmark(child)});
      }
    }
    else {
      let result, oldViewer;
      try {
        let wasOpen = aNode.containerOpen;
        if (!wasOpen) {
          result = aNode.parentResult;
          oldViewer = result.viewer;
          result.viewer = null;
          aNode.containerOpen = true;
        }
        for (let i = 0; i < aNode.childCount; ++i) {
          let child = aNode.getChild(i);
          if (this.nodeIsURI(child))
            urls.push({uri: child.uri, isBookmark: this.nodeIsBookmark(child)});
        }
        if (!wasOpen)
          aNode.containerOpen = false;
      }
      finally {
        if (oldViewer)
          result.viewer = oldViewer;
      }
    }

    return urls;
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
    for each (var item in aItemsToOpen) {
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
    var urlsToOpen = this.getURLsForContainerNode(aNode);
    if (!this._confirmOpenInTabs(urlsToOpen.length))
      return;

    this._openTabset(urlsToOpen, aEvent);
  },

  openURINodesInTabs: function PU_openURINodesInTabs(aNodes, aEvent) {
    var urlsToOpen = [];
    for (var i=0; i < aNodes.length; i++) {
      // skip over separators and folders
      if (this.nodeIsURI(aNodes[i]))
        urlsToOpen.push({uri: aNodes[i].uri, isBookmark: this.nodeIsBookmark(aNodes[i])});
    }
    this._openTabset(urlsToOpen, aEvent);
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

      if (this.uriTypes.indexOf(type) != -1) {
        element = document.createElement("menuitem");
        element.setAttribute("statustext", aNode.uri);
        element.className = "menuitem-iconic bookmark-item";
      }
      else if (this.containerTypes.indexOf(type) != -1) {
        element = document.createElement("menu");
        element.setAttribute("container", "true");

        if (aNode.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY)
          element.setAttribute("query", "true");
        if (this.nodeIsLivemarkContainer(aNode))
          element.setAttribute("livemark", "true");
        else if (this.bookmarks
                     .getFolderIdForItem(aNode.itemId) == this.tagsFolderId) {
            element.setAttribute("tagContainer", "true");
        }

        var popup = document.createElement("menupopup");
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
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch2);
    var leftPaneRoot = -1;
    var allBookmarksId;
    try {
      leftPaneRoot = prefs.getIntPref("browser.places.leftPaneFolderId");
      // if the pref is set to -1 then we must create a new root because we have a new places.sqlite
      if (leftPaneRoot != -1) {
        // Build the leftPaneQueries Map
        delete this.leftPaneQueries;
        this.leftPaneQueries = {};
        var items = this.annotations.getItemsWithAnnotation(ORGANIZER_QUERY_ANNO, { });
        for (var i=0; i < items.length; i++) {
          var queryName = this.annotations
                              .getItemAnnotation(items[i], ORGANIZER_QUERY_ANNO);
          this.leftPaneQueries[queryName] = items[i];
        }
        delete this.leftPaneFolderId;
        return this.leftPaneFolderId = leftPaneRoot;
      }
    }
    catch (ex) { }

    var self = this;
    const EXPIRE_NEVER = this.annotations.EXPIRE_NEVER;
    var callback = {
      runBatched: function(aUserData) {
        delete self.leftPaneQueries;
        self.leftPaneQueries = { };

        // Left Pane Root Folder
        leftPaneRoot = self.bookmarks.createFolder(self.placesRootId, "", -1);

        // History Query
        let uri = IO.newURI("place:sort=4&");
        let title = self.getString("OrganizerQueryHistory");
        let itemId = self.bookmarks.insertBookmark(leftPaneRoot, uri, -1, title);
        self.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                           "History", 0, EXPIRE_NEVER);
        self.leftPaneQueries["History"] = itemId;

        // XXX: Downloads

        // Tags Query
        uri = IO.newURI("place:folder=" + self.tagsFolderId);
        itemId = self.bookmarks.insertBookmark(leftPaneRoot, uri, -1, null);
        self.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                           "Tags", 0, EXPIRE_NEVER);
        self.leftPaneQueries["Tags"] = itemId;

        // All Bookmarks Folder
        title = self.getString("OrganizerQueryAllBookmarks");
        itemId = self.bookmarks.createFolder(leftPaneRoot, title, -1);
        allBookmarksId = itemId;
        self.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                           "AllBookmarks", 0, EXPIRE_NEVER);
        self.leftPaneQueries["AllBookmarks"] = itemId;

        // All Bookmarks->Bookmarks Toolbar Query
        uri = IO.newURI("place:folder=" + self.toolbarFolderId);
        itemId = self.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        self.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                           "BookmarksToolbar", 0, EXPIRE_NEVER);
        self.leftPaneQueries["BookmarksToolbar"] = itemId;

        // All Bookmarks->Bookmarks Menu Query
        uri = IO.newURI("place:folder=" + self.bookmarksMenuFolderId);
        itemId = self.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        self.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                           "BookmarksMenu", 0, EXPIRE_NEVER);
        self.leftPaneQueries["BookmarksMenu"] = itemId;

        // All Bookmarks->Unfiled bookmarks
        uri = IO.newURI("place:folder=" + self.unfiledBookmarksFolderId);
        itemId = self.bookmarks.insertBookmark(allBookmarksId, uri, -1, null);
        self.annotations.setItemAnnotation(itemId, ORGANIZER_QUERY_ANNO,
                                           "UnfiledBookmarks", 0,
                                           EXPIRE_NEVER);
        self.leftPaneQueries["UnfiledBookmarks"] = itemId;

        // disallow manipulating this folder within the organizer UI
        self.bookmarks.setFolderReadonly(leftPaneRoot, true);
      }
    };
    this.bookmarks.runInBatchMode(callback, null);
    prefs.setIntPref("browser.places.leftPaneFolderId", leftPaneRoot);
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

PlacesUtils.placesFlavors = [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                             PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
                             PlacesUtils.TYPE_X_MOZ_PLACE];

PlacesUtils.GENERIC_VIEW_DROP_TYPES = [PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER,
                                       PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR,
                                       PlacesUtils.TYPE_X_MOZ_PLACE,
                                       PlacesUtils.TYPE_X_MOZ_URL,
                                       PlacesUtils.TYPE_UNICODE];
