/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "ForgetAboutSite",
                                  "resource://gre/modules/ForgetAboutSite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

/**
 * Represents an insertion point within a container where we can insert
 * items.
 * @param {object} an object containing the following properties:
 *   - parentId
 *     The identifier of the parent container
 *   - parentGuid
 *     The unique identifier of the parent container
 *   - index
 *     The index within the container where to insert, defaults to appending
 *   - orientation
 *     The orientation of the insertion. NOTE: the adjustments to the
 *     insertion point to accommodate the orientation should be done by
 *     the person who constructs the IP, not the user. The orientation
 *     is provided for informational purposes only! Defaults to DROP_ON.
 *   - tagName
 *     The tag name if this IP is set to a tag, null otherwise.
 *   - dropNearNode
 *     When defined index will be calculated based on this node
 */
function InsertionPoint({ parentId, parentGuid,
                          index = PlacesUtils.bookmarks.DEFAULT_INDEX,
                          orientation = Components.interfaces.nsITreeView.DROP_ON,
                          tagName = null,
                          dropNearNode = null }) {
  this.itemId = parentId;
  this.guid = parentGuid;
  this._index = index;
  this.orientation = orientation;
  this.tagName = tagName;
  this.dropNearNode = dropNearNode;
}

InsertionPoint.prototype = {
  set index(val) {
    return this._index = val;
  },

  async getIndex() {
    if (this.dropNearNode) {
      // If dropNearNode is set up we must calculate the index of the item near
      // which we will drop.
      let index = (await PlacesUtils.bookmarks.fetch(this.dropNearNode.bookmarkGuid)).index;
      return this.orientation == Ci.nsITreeView.DROP_BEFORE ? index : index + 1;
    }
    return this._index;
  },

  get isTag() {
    return typeof(this.tagName) == "string";
  }
};

/**
 * Places Controller
 */

function PlacesController(aView) {
  this._view = aView;
  XPCOMUtils.defineLazyServiceGetter(this, "clipboard",
                                     "@mozilla.org/widget/clipboard;1",
                                     "nsIClipboard");
  XPCOMUtils.defineLazyGetter(this, "profileName", function() {
    return Services.dirsvc.get("ProfD", Ci.nsIFile).leafName;
  });

  this._cachedLivemarkInfoObjects = new Map();
}

PlacesController.prototype = {
  /**
   * The places view.
   */
  _view: null,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIClipboardOwner
  ]),

  // nsIClipboardOwner
  LosingOwnership: function PC_LosingOwnership(aXferable) {
    this.cutNodes = [];
  },

  terminate: function PC_terminate() {
    this._releaseClipboardOwnership();
  },

  supportsCommand: function PC_supportsCommand(aCommand) {
    // Non-Places specific commands that we also support
    switch (aCommand) {
    case "cmd_undo":
    case "cmd_redo":
    case "cmd_cut":
    case "cmd_copy":
    case "cmd_paste":
    case "cmd_delete":
    case "cmd_selectAll":
      return true;
    }

    // All other Places Commands are prefixed with "placesCmd_" ... this
    // filters out other commands that we do _not_ support (see 329587).
    const CMD_PREFIX = "placesCmd_";
    return (aCommand.substr(0, CMD_PREFIX.length) == CMD_PREFIX);
  },

  isCommandEnabled: function PC_isCommandEnabled(aCommand) {
    switch (aCommand) {
    case "cmd_undo":
      if (!PlacesUIUtils.useAsyncTransactions)
        return PlacesUtils.transactionManager.numberOfUndoItems > 0;

      return PlacesTransactions.topUndoEntry != null;
    case "cmd_redo":
      if (!PlacesUIUtils.useAsyncTransactions)
        return PlacesUtils.transactionManager.numberOfRedoItems > 0;

      return PlacesTransactions.topRedoEntry != null;
    case "cmd_cut":
    case "placesCmd_cut":
    case "placesCmd_moveBookmarks":
      for (let node of this._view.selectedNodes) {
        // If selection includes history nodes or tags-as-bookmark, disallow
        // cutting.
        if (node.itemId == -1 ||
            (node.parent && PlacesUtils.nodeIsTagQuery(node.parent))) {
          return false;
        }
      }
      // Otherwise fall through the cmd_delete check.
    case "cmd_delete":
    case "placesCmd_delete":
    case "placesCmd_deleteDataHost":
      return this._hasRemovableSelection();
    case "cmd_copy":
    case "placesCmd_copy":
      return this._view.hasSelection;
    case "cmd_paste":
    case "placesCmd_paste":
      return this._canInsert(true) && this._isClipboardDataPasteable();
    case "cmd_selectAll":
      if (this._view.selType != "single") {
        let rootNode = this._view.result.root;
        if (rootNode.containerOpen && rootNode.childCount > 0)
          return true;
      }
      return false;
    case "placesCmd_open":
    case "placesCmd_open:window":
    case "placesCmd_open:privatewindow":
    case "placesCmd_open:tab": {
      let selectedNode = this._view.selectedNode;
      return selectedNode && PlacesUtils.nodeIsURI(selectedNode);
    }
    case "placesCmd_new:folder":
      return this._canInsert();
    case "placesCmd_new:bookmark":
      return this._canInsert();
    case "placesCmd_new:separator":
      return this._canInsert() &&
             !PlacesUtils.asQuery(this._view.result.root).queryOptions.excludeItems &&
             this._view.result.sortingMode ==
                 Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
    case "placesCmd_show:info": {
      let selectedNode = this._view.selectedNode;
      return selectedNode && PlacesUtils.getConcreteItemId(selectedNode) != -1;
    }
    case "placesCmd_reload": {
      // Livemark containers
      let selectedNode = this._view.selectedNode;
      return selectedNode && this.hasCachedLivemarkInfo(selectedNode);
    }
    case "placesCmd_sortBy:name": {
      let selectedNode = this._view.selectedNode;
      return selectedNode &&
             PlacesUtils.nodeIsFolder(selectedNode) &&
             !PlacesUIUtils.isFolderReadOnly(selectedNode, this._view) &&
             this._view.result.sortingMode ==
                 Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
    }
    case "placesCmd_createBookmark":
      var node = this._view.selectedNode;
      return node && PlacesUtils.nodeIsURI(node) && node.itemId == -1;
    default:
      return false;
    }
  },

  doCommand: function PC_doCommand(aCommand) {
    switch (aCommand) {
    case "cmd_undo":
      if (!PlacesUIUtils.useAsyncTransactions) {
        PlacesUtils.transactionManager.undoTransaction();
        return;
      }
      PlacesTransactions.undo().catch(Components.utils.reportError);
      break;
    case "cmd_redo":
      if (!PlacesUIUtils.useAsyncTransactions) {
        PlacesUtils.transactionManager.redoTransaction();
        return;
      }
      PlacesTransactions.redo().catch(Components.utils.reportError);
      break;
    case "cmd_cut":
    case "placesCmd_cut":
      this.cut();
      break;
    case "cmd_copy":
    case "placesCmd_copy":
      this.copy();
      break;
    case "cmd_paste":
    case "placesCmd_paste":
      this.paste().catch(Components.utils.reportError);
      break;
    case "cmd_delete":
    case "placesCmd_delete":
      this.remove("Remove Selection").catch(Components.utils.reportError);
      break;
    case "placesCmd_deleteDataHost":
      var host;
      if (PlacesUtils.nodeIsHost(this._view.selectedNode)) {
        var queries = this._view.selectedNode.getQueries();
        host = queries[0].domain;
      } else
        host = NetUtil.newURI(this._view.selectedNode.uri).host;
      ForgetAboutSite.removeDataFromDomain(host)
                     .catch(Components.utils.reportError);
      break;
    case "cmd_selectAll":
      this.selectAll();
      break;
    case "placesCmd_open":
      PlacesUIUtils.openNodeIn(this._view.selectedNode, "current", this._view);
      break;
    case "placesCmd_open:window":
      PlacesUIUtils.openNodeIn(this._view.selectedNode, "window", this._view);
      break;
    case "placesCmd_open:privatewindow":
      PlacesUIUtils.openNodeIn(this._view.selectedNode, "window", this._view, true);
      break;
    case "placesCmd_open:tab":
      PlacesUIUtils.openNodeIn(this._view.selectedNode, "tab", this._view);
      break;
    case "placesCmd_new:folder":
      this.newItem("folder").catch(Components.utils.reportError);
      break;
    case "placesCmd_new:bookmark":
      this.newItem("bookmark").catch(Components.utils.reportError);
      break;
    case "placesCmd_new:separator":
      this.newSeparator().catch(Components.utils.reportError);
      break;
    case "placesCmd_show:info":
      this.showBookmarkPropertiesForSelection();
      break;
    case "placesCmd_moveBookmarks":
      this.moveSelectedBookmarks().catch(Components.utils.reportError);
      break;
    case "placesCmd_reload":
      this.reloadSelectedLivemark();
      break;
    case "placesCmd_sortBy:name":
      this.sortFolderByName().catch(Components.utils.reportError);
      break;
    case "placesCmd_createBookmark":
      let node = this._view.selectedNode;
      PlacesUIUtils.showBookmarkDialog({ action: "add",
                                         type: "bookmark",
                                         hiddenRows: [ "description",
                                                        "keyword",
                                                        "location",
                                                        "loadInSidebar" ],
                                         uri: NetUtil.newURI(node.uri),
                                         title: node.title
                                       }, window.top);
      break;
    }
  },

  onEvent: function PC_onEvent(eventName) { },


  /**
   * Determine whether or not the selection can be removed, either by the
   * delete or cut operations based on whether or not any of its contents
   * are non-removable. We don't need to worry about recursion here since it
   * is a policy decision that a removable item not be placed inside a non-
   * removable item.
   *
   * @return true if all nodes in the selection can be removed,
   *         false otherwise.
   */
  _hasRemovableSelection() {
    var ranges = this._view.removableSelectionRanges;
    if (!ranges.length)
      return false;

    var root = this._view.result.root;

    for (var j = 0; j < ranges.length; j++) {
      var nodes = ranges[j];
      for (var i = 0; i < nodes.length; ++i) {
        // Disallow removing the view's root node
        if (nodes[i] == root)
          return false;

        if (!PlacesUIUtils.canUserRemove(nodes[i], this._view))
          return false;
      }
    }

    return true;
  },

  /**
   * Determines whether or not nodes can be inserted relative to the selection.
   */
  _canInsert: function PC__canInsert(isPaste) {
    var ip = this._view.insertionPoint;
    return ip != null && (isPaste || ip.isTag != true);
  },

  /**
   * Looks at the data on the clipboard to see if it is paste-able.
   * Paste-able data is:
   *   - in a format that the view can receive
   * @return true if: - clipboard data is of a TYPE_X_MOZ_PLACE_* flavor,
   *                  - clipboard data is of type TEXT_UNICODE and
   *                    is a valid URI.
   */
  _isClipboardDataPasteable: function PC__isClipboardDataPasteable() {
    // if the clipboard contains TYPE_X_MOZ_PLACE_* data, it is definitely
    // pasteable, with no need to unwrap all the nodes.

    var flavors = PlacesUIUtils.PLACES_FLAVORS;
    var clipboard = this.clipboard;
    var hasPlacesData =
      clipboard.hasDataMatchingFlavors(flavors, flavors.length,
                                       Ci.nsIClipboard.kGlobalClipboard);
    if (hasPlacesData)
      return this._view.insertionPoint != null;

    // if the clipboard doesn't have TYPE_X_MOZ_PLACE_* data, we also allow
    // pasting of valid "text/unicode" and "text/x-moz-url" data
    var xferable = Cc["@mozilla.org/widget/transferable;1"].
                   createInstance(Ci.nsITransferable);
    xferable.init(null);

    xferable.addDataFlavor(PlacesUtils.TYPE_X_MOZ_URL);
    xferable.addDataFlavor(PlacesUtils.TYPE_UNICODE);
    clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);

    try {
      // getAnyTransferData will throw if no data is available.
      var data = { }, type = { };
      xferable.getAnyTransferData(type, data, { });
      data = data.value.QueryInterface(Ci.nsISupportsString).data;
      if (type.value != PlacesUtils.TYPE_X_MOZ_URL &&
          type.value != PlacesUtils.TYPE_UNICODE)
        return false;

      // unwrapNodes() will throw if the data blob is malformed.
      PlacesUtils.unwrapNodes(data, type.value);
      return this._view.insertionPoint != null;
    } catch (e) {
      // getAnyTransferData or unwrapNodes failed
      return false;
    }
  },

  /**
   * Gathers information about the selected nodes according to the following
   * rules:
   *    "link"              node is a URI
   *    "bookmark"          node is a bookmark
   *    "livemarkChild"     node is a child of a livemark
   *    "tagChild"          node is a child of a tag
   *    "folder"            node is a folder
   *    "query"             node is a query
   *    "separator"         node is a separator line
   *    "host"              node is a host
   *
   * @return an array of objects corresponding the selected nodes. Each
   *         object has each of the properties above set if its corresponding
   *         node matches the rule. In addition, the annotations names for each
   *         node are set on its corresponding object as properties.
   * Notes:
   *   1) This can be slow, so don't call it anywhere performance critical!
   */
  _buildSelectionMetadata: function PC__buildSelectionMetadata() {
    var metadata = [];
    var nodes = this._view.selectedNodes;

    for (var i = 0; i < nodes.length; i++) {
      var nodeData = {};
      var node = nodes[i];
      var nodeType = node.type;
      var uri = null;

      // We don't use the nodeIs* methods here to avoid going through the type
      // property way too often
      switch (nodeType) {
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY:
          nodeData.query = true;
          if (node.parent) {
            switch (PlacesUtils.asQuery(node.parent).queryOptions.resultType) {
              case Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY:
                nodeData.host = true;
                break;
              case Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_SITE_QUERY:
              case Ci.nsINavHistoryQueryOptions.RESULTS_AS_DATE_QUERY:
                nodeData.day = true;
                break;
            }
          }
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER:
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER_SHORTCUT:
          nodeData.folder = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR:
          nodeData.separator = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_URI:
          nodeData.link = true;
          uri = NetUtil.newURI(node.uri);
          if (PlacesUtils.nodeIsBookmark(node)) {
            nodeData.bookmark = true;
            var parentNode = node.parent;
            if (parentNode) {
              if (PlacesUtils.nodeIsTagQuery(parentNode))
                nodeData.tagChild = true;
              else if (this.hasCachedLivemarkInfo(parentNode))
                nodeData.livemarkChild = true;
            }
          }
          break;
      }

      // annotations
      if (uri) {
        let names = PlacesUtils.annotations.getPageAnnotationNames(uri);
        for (let j = 0; j < names.length; ++j)
          nodeData[names[j]] = true;
      }

      // For items also include the item-specific annotations
      if (node.itemId != -1) {
        let names = PlacesUtils.annotations
                               .getItemAnnotationNames(node.itemId);
        for (let j = 0; j < names.length; ++j)
          nodeData[names[j]] = true;
      }
      metadata.push(nodeData);
    }

    return metadata;
  },

  /**
   * Determines if a context-menu item should be shown
   * @param   aMenuItem
   *          the context menu item
   * @param   aMetaData
   *          meta data about the selection
   * @return true if the conditions (see buildContextMenu) are satisfied
   *         and the item can be displayed, false otherwise.
   */
  _shouldShowMenuItem: function PC__shouldShowMenuItem(aMenuItem, aMetaData) {
    var selectiontype = aMenuItem.getAttribute("selectiontype");
    if (!selectiontype) {
      selectiontype = "single|multiple";
    }
    var selectionTypes = selectiontype.split("|");
    if (selectionTypes.includes("any")) {
      return true;
    }
    var count = aMetaData.length;
    if (count > 1 && !selectionTypes.includes("multiple"))
      return false;
    if (count == 1 && !selectionTypes.includes("single"))
      return false;
    // NB: if there is no selection, we show the item if and only if
    // the selectiontype includes 'none' - the metadata list will be
    // empty so none of the other criteria will apply anyway.
    if (count == 0)
      return selectionTypes.includes("none");

    var forceHideAttr = aMenuItem.getAttribute("forcehideselection");
    if (forceHideAttr) {
      var forceHideRules = forceHideAttr.split("|");
      for (let i = 0; i < aMetaData.length; ++i) {
        for (let j = 0; j < forceHideRules.length; ++j) {
          if (forceHideRules[j] in aMetaData[i])
            return false;
        }
      }
    }

    var selectionAttr = aMenuItem.getAttribute("selection");
    if (!selectionAttr) {
      return !aMenuItem.hidden;
    }

    if (selectionAttr == "any")
      return true;

    var showRules = selectionAttr.split("|");
    var anyMatched = false;
    function metaDataNodeMatches(metaDataNode, rules) {
      for (var i = 0; i < rules.length; i++) {
        if (rules[i] in metaDataNode)
          return true;
      }
      return false;
    }

    for (var i = 0; i < aMetaData.length; ++i) {
      if (metaDataNodeMatches(aMetaData[i], showRules))
        anyMatched = true;
      else
        return false;
    }
    return anyMatched;
  },

  /**
   * Detects information (meta-data rules) about the current selection in the
   * view (see _buildSelectionMetadata) and sets the visibility state for each
   * of the menu-items in the given popup with the following rules applied:
   *  0) The "ignoreitem" attribute may be set to "true" for this code not to
   *     handle that menuitem.
   *  1) The "selectiontype" attribute may be set on a menu-item to "single"
   *     if the menu-item should be visible only if there is a single node
   *     selected, or to "multiple" if the menu-item should be visible only if
   *     multiple nodes are selected, or to "none" if the menuitems should be
   *     visible for if there are no selected nodes, or to a |-separated
   *     combination of these.
   *     If the attribute is not set or set to an invalid value, the menu-item
   *     may be visible irrespective of the selection.
   *  2) The "selection" attribute may be set on a menu-item to the various
   *     meta-data rules for which it may be visible. The rules should be
   *     separated with the | character.
   *  3) A menu-item may be visible only if at least one of the rules set in
   *     its selection attribute apply to each of the selected nodes in the
   *     view.
   *  4) The "forcehideselection" attribute may be set on a menu-item to rules
   *     for which it should be hidden. This attribute takes priority over the
   *     selection attribute. A menu-item would be hidden if at least one of the
   *     given rules apply to one of the selected nodes. The rules should be
   *     separated with the | character.
   *  5) The "hideifnoinsertionpoint" attribute may be set on a menu-item to
   *     true if it should be hidden when there's no insertion point
   *  6) The visibility state of a menu-item is unchanged if none of these
   *     attribute are set.
   *  7) These attributes should not be set on separators for which the
   *     visibility state is "auto-detected."
   *  8) The "hideifprivatebrowsing" attribute may be set on a menu-item to
   *     true if it should be hidden inside the private browsing mode
   * @param   aPopup
   *          The menupopup to build children into.
   * @return true if at least one item is visible, false otherwise.
   */
  buildContextMenu: function PC_buildContextMenu(aPopup) {
    var metadata = this._buildSelectionMetadata();
    var ip = this._view.insertionPoint;
    var noIp = !ip || ip.isTag;

    var separator = null;
    var visibleItemsBeforeSep = false;
    var usableItemCount = 0;
    for (var i = 0; i < aPopup.childNodes.length; ++i) {
      var item = aPopup.childNodes[i];
      if (item.getAttribute("ignoreitem") == "true") {
        continue;
      }
      if (item.localName != "menuseparator") {
        // We allow pasting into tag containers, so special case that.
        var hideIfNoIP = item.getAttribute("hideifnoinsertionpoint") == "true" &&
                         noIp && !(ip && ip.isTag && item.id == "placesContext_paste");
        var hideIfPrivate = item.getAttribute("hideifprivatebrowsing") == "true" &&
                            PrivateBrowsingUtils.isWindowPrivate(window);
        var shouldHideItem = hideIfNoIP || hideIfPrivate ||
                             !this._shouldShowMenuItem(item, metadata);
        item.hidden = item.disabled = shouldHideItem;

        if (!item.hidden) {
          visibleItemsBeforeSep = true;
          usableItemCount++;

          // Show the separator above the menu-item if any
          if (separator) {
            separator.hidden = false;
            separator = null;
          }
        }
      } else { // menuseparator
        // Initially hide it. It will be unhidden if there will be at least one
        // visible menu-item above and below it.
        item.hidden = true;

        // We won't show the separator at all if no items are visible above it
        if (visibleItemsBeforeSep)
          separator = item;

        // New separator, count again:
        visibleItemsBeforeSep = false;
      }
    }

    // Set Open Folder/Links In Tabs items enabled state if they're visible
    if (usableItemCount > 0) {
      var openContainerInTabsItem = document.getElementById("placesContext_openContainer:tabs");
      if (!openContainerInTabsItem.hidden) {
        var containerToUse = this._view.selectedNode || this._view.result.root;
        if (PlacesUtils.nodeIsContainer(containerToUse)) {
          if (!PlacesUtils.hasChildURIs(containerToUse)) {
            openContainerInTabsItem.disabled = true;
            // Ensure that we don't display the menu if nothing is enabled:
            usableItemCount--;
          }
        }
      }
    }

    // Make sure correct PluralForms are diplayed when multiple pages are selected.
    var deleteHistoryItem = document.getElementById("placesContext_delete_history");
    deleteHistoryItem.label = PlacesUIUtils.getPluralString("cmd.deletePages.label",
                                                            metadata.length);
    deleteHistoryItem.accessKey = PlacesUIUtils.getString("cmd.deletePages.accesskey");
    var createBookmarkItem = document.getElementById("placesContext_createBookmark");
    createBookmarkItem.label = PlacesUIUtils.getPluralString("cmd.bookmarkPages.label",
                                                             metadata.length);
    createBookmarkItem.accessKey = PlacesUIUtils.getString("cmd.bookmarkPages.accesskey");

    return usableItemCount > 0;
  },

  /**
   * Select all links in the current view.
   */
  selectAll: function PC_selectAll() {
    this._view.selectAll();
  },

  /**
   * Opens the bookmark properties for the selected URI Node.
   */
  showBookmarkPropertiesForSelection() {
    let node = this._view.selectedNode;
    if (!node)
      return;

    PlacesUIUtils.showBookmarkDialog({ action: "edit",
                                       node,
                                       hiddenRows: [ "folderPicker" ]
                                     }, window.top);
  },

  /**
   * This method can be run on a URI parameter to ensure that it didn't
   * receive a string instead of an nsIURI object.
   */
  _assertURINotString: function PC__assertURINotString(value) {
    NS_ASSERT((typeof(value) == "object") && !(value instanceof String),
           "This method should be passed a URI as a nsIURI object, not as a string.");
  },

  /**
   * Reloads the selected livemark if any.
   */
  reloadSelectedLivemark: function PC_reloadSelectedLivemark() {
    var selectedNode = this._view.selectedNode;
    if (selectedNode) {
      let itemId = selectedNode.itemId;
      PlacesUtils.livemarks.getLivemark({ id: itemId })
        .then(aLivemark => {
          aLivemark.reload(true);
        }, Components.utils.reportError);
    }
  },

  /**
   * Opens the links in the selected folder, or the selected links in new tabs.
   */
  openSelectionInTabs: function PC_openLinksInTabs(aEvent) {
    var node = this._view.selectedNode;
    var nodes = this._view.selectedNodes;
    // In the case of no selection, open the root node:
    if (!node && !nodes.length) {
      node = this._view.result.root;
    }
    if (node && PlacesUtils.nodeIsContainer(node))
      PlacesUIUtils.openContainerNodeInTabs(node, aEvent, this._view);
    else
      PlacesUIUtils.openURINodesInTabs(nodes, aEvent, this._view);
  },

  /**
   * Shows the Add Bookmark UI for the current insertion point.
   *
   * @param aType
   *        the type of the new item (bookmark/livemark/folder)
   */
  async newItem(aType) {
    let ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    let performed =
      PlacesUIUtils.showBookmarkDialog({ action: "add",
                                         type: aType,
                                         defaultInsertionPoint: ip,
                                         hiddenRows: [ "folderPicker" ]
                                       }, window.top);
    if (performed) {
      // Select the new item.
      let insertedNodeId = PlacesUtils.bookmarks
                                      .getIdForItemAt(ip.itemId, await ip.getIndex());
      this._view.selectItems([insertedNodeId], false);
    }
  },

  /**
   * Create a new Bookmark separator somewhere.
   */
  async newSeparator() {
    var ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    let index = await ip.getIndex();
    if (!PlacesUIUtils.useAsyncTransactions) {
      let txn = new PlacesCreateSeparatorTransaction(ip.itemId, index);
      PlacesUtils.transactionManager.doTransaction(txn);
      // Select the new item.
      let insertedNodeId = PlacesUtils.bookmarks
                                      .getIdForItemAt(ip.itemId, index);
      this._view.selectItems([insertedNodeId], false);
      return;
    }

    let txn = PlacesTransactions.NewSeparator({ parentGuid: ip.guid, index });
    let guid = await txn.transact();
    let itemId = await PlacesUtils.promiseItemId(guid);
    // Select the new item.
    this._view.selectItems([itemId], false);
  },

  /**
   * Opens a dialog for moving the selected nodes.
   */
  async moveSelectedBookmarks() {
    let args = {
      // The guid of the folder to move bookmarks to. This will only be
      // set in the useAsyncTransactions case.
      moveToGuid: null,
      // nodes is passed to support !useAsyncTransactions.
      nodes: this._view.selectedNodes,
    };
    window.openDialog("chrome://browser/content/places/moveBookmarks.xul",
                      "", "chrome, modal",
                      args);

    if (!args.moveToGuid) {
      return;
    }

    let transactions = [];

    for (let node of this._view.selectedNodes) {
      // Nothing to do if the node is already under the selected folder.
      if (node.parent.bookmarkGuid == args.moveToGuid) {
        continue;
      }
      transactions.push(PlacesTransactions.Move({
        guid: node.bookmarkGuid,
        newParentGuid: args.moveToGuid,
      }));
    }

    if (transactions.length) {
      await PlacesUIUtils.batchUpdatesForNode(this._view.result, transactions.length, async () => {
        await PlacesTransactions.batch(transactions);
      });
    }
  },

  /**
   * Sort the selected folder by name
   */
  async sortFolderByName() {
    let itemId = PlacesUtils.getConcreteItemId(this._view.selectedNode);
    if (!PlacesUIUtils.useAsyncTransactions) {
      var txn = new PlacesSortFolderByNameTransaction(itemId);
      PlacesUtils.transactionManager.doTransaction(txn);
      return;
    }
    let guid = await PlacesUtils.promiseItemGuid(itemId);
    await PlacesTransactions.SortByName(guid).transact();
  },

  /**
   * Walk the list of folders we're removing in this delete operation, and
   * see if the selected node specified is already implicitly being removed
   * because it is a child of that folder.
   * @param   node
   *          Node to check for containment.
   * @param   pastFolders
   *          List of folders the calling function has already traversed
   * @return true if the node should be skipped, false otherwise.
   */
  _shouldSkipNode: function PC_shouldSkipNode(node, pastFolders) {
    /**
     * Determines if a node is contained by another node within a resultset.
     * @param   node
     *          The node to check for containment for
     * @param   parent
     *          The parent container to check for containment in
     * @return true if node is a member of parent's children, false otherwise.
     */
    function isNodeContainedBy(parent) {
      var cursor = node.parent;
      while (cursor) {
        if (cursor == parent)
          return true;
        cursor = cursor.parent;
      }
      return false;
    }

    for (var j = 0; j < pastFolders.length; ++j) {
      if (isNodeContainedBy(pastFolders[j]))
        return true;
    }
    return false;
  },

  /**
   * Creates a set of transactions for the removal of a range of items.
   * A range is an array of adjacent nodes in a view.
   * @param   [in] range
   *          An array of nodes to remove. Should all be adjacent.
   * @param   [out] transactions
   *          An array of transactions.
   * @param   [optional] removedFolders
   *          An array of folder nodes that have already been removed.
   * @return {Integer} The total number of items affected.
   */
  async _removeRange(range, transactions, removedFolders) {
    NS_ASSERT(transactions instanceof Array, "Must pass a transactions array");
    if (!removedFolders)
      removedFolders = [];

    let bmGuidsToRemove = [];
    let totalItems = 0;

    for (var i = 0; i < range.length; ++i) {
      var node = range[i];
      if (this._shouldSkipNode(node, removedFolders))
        continue;

      totalItems++;

      if (PlacesUtils.nodeIsTagQuery(node.parent)) {
        // This is a uri node inside a tag container.  It needs a special
        // untag transaction.
        var tagItemId = PlacesUtils.getConcreteItemId(node.parent);
        var uri = NetUtil.newURI(node.uri);
        if (PlacesUIUtils.useAsyncTransactions) {
          let tag = node.parent.title;
          if (!tag) {
            let tagGuid = await PlacesUtils.promiseItemGuid(tagItemId);
            tag = (await PlacesUtils.bookmarks.fetch(tagGuid)).title;
          }
          transactions.push(PlacesTransactions.Untag({ urls: [uri], tag }));
        } else {
          let txn = new PlacesUntagURITransaction(uri, [tagItemId]);
          transactions.push(txn);
        }
      } else if (PlacesUtils.nodeIsTagQuery(node) && node.parent &&
               PlacesUtils.nodeIsQuery(node.parent) &&
               PlacesUtils.asQuery(node.parent).queryOptions.resultType ==
                 Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_QUERY) {
        // This is a tag container.
        // Untag all URIs tagged with this tag only if the tag container is
        // child of the "Tags" query in the library, in all other places we
        // must only remove the query node.
        let tag = node.title;
        let URIs = PlacesUtils.tagging.getURIsForTag(tag);
        if (PlacesUIUtils.useAsyncTransactions) {
          transactions.push(PlacesTransactions.Untag({ tag, urls: URIs }));
        } else {
          for (var j = 0; j < URIs.length; j++) {
            let txn = new PlacesUntagURITransaction(URIs[j], [tag]);
            transactions.push(txn);
          }
        }
      } else if (PlacesUtils.nodeIsURI(node) &&
               PlacesUtils.nodeIsQuery(node.parent) &&
               PlacesUtils.asQuery(node.parent).queryOptions.queryType ==
                 Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY) {
        // This is a uri node inside an history query.
        PlacesUtils.history.remove(node.uri).catch(Components.utils.reportError);
        // History deletes are not undoable, so we don't have a transaction.
      } else if (node.itemId == -1 &&
               PlacesUtils.nodeIsQuery(node) &&
               PlacesUtils.asQuery(node).queryOptions.queryType ==
                 Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY) {
        // This is a dynamically generated history query, like queries
        // grouped by site, time or both.  Dynamically generated queries don't
        // have an itemId even if they are descendants of a bookmark.
        this._removeHistoryContainer(node);
        // History deletes are not undoable, so we don't have a transaction.
      } else {
        // This is a common bookmark item.
        if (PlacesUtils.nodeIsFolder(node)) {
          // If this is a folder we add it to our array of folders, used
          // to skip nodes that are children of an already removed folder.
          removedFolders.push(node);
        }
        if (PlacesUIUtils.useAsyncTransactions) {
          bmGuidsToRemove.push(node.bookmarkGuid);
        } else {
          let txn = new PlacesRemoveItemTransaction(node.itemId);
          transactions.push(txn);
        }
      }
    }
    if (bmGuidsToRemove.length) {
      transactions.push(PlacesTransactions.Remove({ guids: bmGuidsToRemove }));
    }
    return totalItems;
  },

  /**
   * Removes the set of selected ranges from bookmarks.
   * @param   txnName
   *          See |remove|.
   */
  async _removeRowsFromBookmarks(txnName) {
    let ranges = this._view.removableSelectionRanges;
    let transactions = [];
    let removedFolders = [];
    let totalItems = 0;

    for (let range of ranges) {
      totalItems += await this._removeRange(range, transactions, removedFolders);
    }

    if (transactions.length > 0) {
      if (PlacesUIUtils.useAsyncTransactions) {
        await PlacesUIUtils.batchUpdatesForNode(this._view.result, totalItems, async () => {
          await PlacesTransactions.batch(transactions);
        });
      } else {
        var txn = new PlacesAggregatedTransaction(txnName, transactions);
        PlacesUtils.transactionManager.doTransaction(txn);
      }
    }
  },

  /**
   * Removes the set of selected ranges from history, asynchronously.
   *
   * @note history deletes are not undoable.
   */
  _removeRowsFromHistory: function PC__removeRowsFromHistory() {
    let nodes = this._view.selectedNodes;
    let URIs = new Set();
    for (let i = 0; i < nodes.length; ++i) {
      let node = nodes[i];
      if (PlacesUtils.nodeIsURI(node)) {
        URIs.add(node.uri);
      } else if (PlacesUtils.nodeIsQuery(node) &&
               PlacesUtils.asQuery(node).queryOptions.queryType ==
                 Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY) {
        this._removeHistoryContainer(node);
      }
    }

    PlacesUtils.history.remove([...URIs]).catch(Components.utils.reportError);
  },

  /**
   * Removes history visits for an history container node.
   * @param   [in] aContainerNode
   *          The container node to remove.
   *
   * @note history deletes are not undoable.
   */
  _removeHistoryContainer: function PC__removeHistoryContainer(aContainerNode) {
    if (PlacesUtils.nodeIsHost(aContainerNode)) {
      // Site container.
      PlacesUtils.history.removePagesFromHost(aContainerNode.title, true);
    } else if (PlacesUtils.nodeIsDay(aContainerNode)) {
      // Day container.
      let query = aContainerNode.getQueries()[0];
      let beginTime = query.beginTime;
      let endTime = query.endTime;
      NS_ASSERT(query && beginTime && endTime,
                "A valid date container query should exist!");
      // We want to exclude beginTime from the removal because
      // removePagesByTimeframe includes both extremes, while date containers
      // exclude the lower extreme.  So, if we would not exclude it, we would
      // end up removing more history than requested.
      PlacesUtils.history.removePagesByTimeframe(beginTime + 1, endTime);
    }
  },

  /**
   * Removes the selection
   * @param   aTxnName
   *          A name for the transaction if this is being performed
   *          as part of another operation.
   */
  async remove(aTxnName) {
    if (!this._hasRemovableSelection())
      return;

    NS_ASSERT(aTxnName !== undefined, "Must supply Transaction Name");

    var root = this._view.result.root;

    if (PlacesUtils.nodeIsFolder(root)) {
      if (PlacesUIUtils.useAsyncTransactions)
        await this._removeRowsFromBookmarks(aTxnName);
      else
        this._removeRowsFromBookmarks(aTxnName);
    } else if (PlacesUtils.nodeIsQuery(root)) {
      var queryType = PlacesUtils.asQuery(root).queryOptions.queryType;
      if (queryType == Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS) {
        if (PlacesUIUtils.useAsyncTransactions)
          await this._removeRowsFromBookmarks(aTxnName);
        else
          this._removeRowsFromBookmarks(aTxnName);
      } else if (queryType == Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY) {
        this._removeRowsFromHistory();
      } else {
        NS_ASSERT(false, "implement support for QUERY_TYPE_UNIFIED");
      }
    } else
      NS_ASSERT(false, "unexpected root");
  },

  /**
   * Fills a DataTransfer object with the content of the selection that can be
   * dropped elsewhere.
   * @param   aEvent
   *          The dragstart event.
   */
  setDataTransfer: function PC_setDataTransfer(aEvent) {
    let dt = aEvent.dataTransfer;

    let result = this._view.result;
    let didSuppressNotifications = result.suppressNotifications;
    if (!didSuppressNotifications)
      result.suppressNotifications = true;

    function addData(type, index, feedURI) {
      let wrapNode = PlacesUtils.wrapNode(node, type, feedURI);
      dt.mozSetDataAt(type, wrapNode, index);
    }

    function addURIData(index, feedURI) {
      addData(PlacesUtils.TYPE_X_MOZ_URL, index, feedURI);
      addData(PlacesUtils.TYPE_UNICODE, index, feedURI);
      addData(PlacesUtils.TYPE_HTML, index, feedURI);
    }

    try {
      let nodes = this._view.draggableSelection;
      for (let i = 0; i < nodes.length; ++i) {
        var node = nodes[i];

        // This order is _important_! It controls how this and other
        // applications select data to be inserted based on type.
        addData(PlacesUtils.TYPE_X_MOZ_PLACE, i);

        // Drop the feed uri for livemark containers
        let livemarkInfo = this.getCachedLivemarkInfo(node);
        if (livemarkInfo) {
          addURIData(i, livemarkInfo.feedURI.spec);
        } else if (node.uri) {
          addURIData(i);
        }
      }
    } finally {
      if (!didSuppressNotifications)
        result.suppressNotifications = false;
    }
  },

  get clipboardAction() {
    let action = {};
    let actionOwner;
    try {
      let xferable = Cc["@mozilla.org/widget/transferable;1"].
                     createInstance(Ci.nsITransferable);
      xferable.init(null);
      xferable.addDataFlavor(PlacesUtils.TYPE_X_MOZ_PLACE_ACTION);
      this.clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);
      xferable.getTransferData(PlacesUtils.TYPE_X_MOZ_PLACE_ACTION, action, {});
      [action, actionOwner] =
        action.value.QueryInterface(Ci.nsISupportsString).data.split(",");
    } catch (ex) {
      // Paste from external sources don't have any associated action, just
      // fallback to a copy action.
      return "copy";
    }
    // For cuts also check who inited the action, since cuts across different
    // instances should instead be handled as copies (The sources are not
    // available for this instance).
    if (action == "cut" && actionOwner != this.profileName)
      action = "copy";

    return action;
  },

  _releaseClipboardOwnership: function PC__releaseClipboardOwnership() {
    if (this.cutNodes.length > 0) {
      // This clears the logical clipboard, doesn't remove data.
      this.clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
    }
  },

  _clearClipboard: function PC__clearClipboard() {
    let xferable = Cc["@mozilla.org/widget/transferable;1"].
                   createInstance(Ci.nsITransferable);
    xferable.init(null);
    // Empty transferables may cause crashes, so just add an unknown type.
    const TYPE = "text/x-moz-place-empty";
    xferable.addDataFlavor(TYPE);
    xferable.setTransferData(TYPE, PlacesUtils.toISupportsString(""), 0);
    this.clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);
  },

  _populateClipboard: function PC__populateClipboard(aNodes, aAction) {
    // This order is _important_! It controls how this and other applications
    // select data to be inserted based on type.
    let contents = [
      { type: PlacesUtils.TYPE_X_MOZ_PLACE, entries: [] },
      { type: PlacesUtils.TYPE_X_MOZ_URL, entries: [] },
      { type: PlacesUtils.TYPE_HTML, entries: [] },
      { type: PlacesUtils.TYPE_UNICODE, entries: [] },
    ];

    // Avoid handling descendants of a copied node, the transactions take care
    // of them automatically.
    let copiedFolders = [];
    aNodes.forEach(function(node) {
      if (this._shouldSkipNode(node, copiedFolders))
        return;
      if (PlacesUtils.nodeIsFolder(node))
        copiedFolders.push(node);

      let livemarkInfo = this.getCachedLivemarkInfo(node);
      let feedURI = livemarkInfo && livemarkInfo.feedURI.spec;

      contents.forEach(function(content) {
        content.entries.push(
          PlacesUtils.wrapNode(node, content.type, feedURI)
        );
      });
    }, this);

    function addData(type, data) {
      xferable.addDataFlavor(type);
      xferable.setTransferData(type, PlacesUtils.toISupportsString(data),
                               data.length * 2);
    }

    let xferable = Cc["@mozilla.org/widget/transferable;1"].
                   createInstance(Ci.nsITransferable);
    xferable.init(null);
    let hasData = false;
    // This order matters here!  It controls how this and other applications
    // select data to be inserted based on type.
    contents.forEach(function(content) {
      if (content.entries.length > 0) {
        hasData = true;
        let glue =
          content.type == PlacesUtils.TYPE_X_MOZ_PLACE ? "," : PlacesUtils.endl;
        addData(content.type, content.entries.join(glue));
      }
    });

    // Track the exected action in the xferable.  This must be the last flavor
    // since it's the least preferred one.
    // Enqueue a unique instance identifier to distinguish operations across
    // concurrent instances of the application.
    addData(PlacesUtils.TYPE_X_MOZ_PLACE_ACTION, aAction + "," + this.profileName);

    if (hasData) {
      this.clipboard.setData(xferable,
                             this.cutNodes.length > 0 ? this : null,
                             Ci.nsIClipboard.kGlobalClipboard);
    }
  },

  _cutNodes: [],
  get cutNodes() {
    return this._cutNodes;
  },
  set cutNodes(aNodes) {
    let self = this;
    function updateCutNodes(aValue) {
      self._cutNodes.forEach(function(aNode) {
        self._view.toggleCutNode(aNode, aValue);
      });
    }

    updateCutNodes(false);
    this._cutNodes = aNodes;
    updateCutNodes(true);
    return aNodes;
  },

  /**
   * Copy Bookmarks and Folders to the clipboard
   */
  copy: function PC_copy() {
    let result = this._view.result;
    let didSuppressNotifications = result.suppressNotifications;
    if (!didSuppressNotifications)
      result.suppressNotifications = true;
    try {
      this._populateClipboard(this._view.selectedNodes, "copy");
    } finally {
      if (!didSuppressNotifications)
        result.suppressNotifications = false;
    }
  },

  /**
   * Cut Bookmarks and Folders to the clipboard
   */
  cut: function PC_cut() {
    let result = this._view.result;
    let didSuppressNotifications = result.suppressNotifications;
    if (!didSuppressNotifications)
      result.suppressNotifications = true;
    try {
      this._populateClipboard(this._view.selectedNodes, "cut");
      this.cutNodes = this._view.selectedNodes;
    } finally {
      if (!didSuppressNotifications)
        result.suppressNotifications = false;
    }
  },

  /**
   * Paste Bookmarks and Folders from the clipboard
   */
  async paste() {
    // No reason to proceed if there isn't a valid insertion point.
    let ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    let action = this.clipboardAction;

    let xferable = Cc["@mozilla.org/widget/transferable;1"].
                   createInstance(Ci.nsITransferable);
    xferable.init(null);
    // This order matters here!  It controls the preferred flavors for this
    // paste operation.
    [ PlacesUtils.TYPE_X_MOZ_PLACE,
      PlacesUtils.TYPE_X_MOZ_URL,
      PlacesUtils.TYPE_UNICODE,
    ].forEach(type => xferable.addDataFlavor(type));

    this.clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);

    // Now get the clipboard contents, in the best available flavor.
    let data = {}, type = {}, items = [];
    try {
      xferable.getAnyTransferData(type, data, {});
      data = data.value.QueryInterface(Ci.nsISupportsString).data;
      type = type.value;
      items = PlacesUtils.unwrapNodes(data, type);
    } catch (ex) {
      // No supported data exists or nodes unwrap failed, just bail out.
      return;
    }

    let itemsToSelect = [];
    if (PlacesUIUtils.useAsyncTransactions) {
      let doCopy = action == "copy";
      let guidsToSelect = await handleTransferItems(items, ip, doCopy, this._view);

      let guidsToIdMap = await PlacesUtils.promiseManyItemIds(guidsToSelect);
      itemsToSelect = Array.from(guidsToIdMap.values());
    } else {
      let transactions = [];
      let insertionIndex = await ip.getIndex();
      for (let index = insertionIndex, i = 0; i < items.length; ++i) {
        if (ip.isTag) {
          // Pasting into a tag container means tagging the item, regardless of
          // the requested action.
          let tagTxn = new PlacesTagURITransaction(NetUtil.newURI(items[i].uri),
                                                   [ip.itemId]);
          transactions.push(tagTxn);
          continue;
        }

        // Adjust index to make sure items are pasted in the correct position.
        // If index is DEFAULT_INDEX, items are just appended.
        if (index != PlacesUtils.bookmarks.DEFAULT_INDEX)
          index += i;

        // If this is not a copy, check for safety that we can move the source,
        // otherwise report an error and fallback to a copy.
        if (action != "copy" && !PlacesControllerDragHelper.canMoveUnwrappedNode(items[i])) {
          Components.utils.reportError("Tried to move an unmovable Places " +
                                       "node, reverting to a copy operation.");
          action = "copy";
        }
        transactions.push(
          PlacesUIUtils.makeTransaction(items[i], type, ip.itemId,
                                        index, action == "copy")
        );
      }

      let aggregatedTxn = new PlacesAggregatedTransaction("Paste", transactions);
      PlacesUtils.transactionManager.doTransaction(aggregatedTxn);

      for (let i = 0; i < transactions.length; ++i) {
        itemsToSelect.push(
          PlacesUtils.bookmarks.getIdForItemAt(ip.itemId, insertionIndex + i)
        );
      }
    }

    // Cut/past operations are not repeatable, so clear the clipboard.
    if (action == "cut") {
      this._clearClipboard();
    }

    if (itemsToSelect.length > 0)
      this._view.selectItems(itemsToSelect, false);
  },

  /**
   * Cache the livemark info for a node.  This allows the controller and the
   * views to treat the given node as a livemark.
   * @param aNode
   *        a places result node.
   * @param aLivemarkInfo
   *        a mozILivemarkInfo object.
   */
  cacheLivemarkInfo: function PC_cacheLivemarkInfo(aNode, aLivemarkInfo) {
    this._cachedLivemarkInfoObjects.set(aNode, aLivemarkInfo);
  },

  /**
   * Returns whether or not there's cached mozILivemarkInfo object for a node.
   * @param aNode
   *        a places result node.
   * @return true if there's a cached mozILivemarkInfo object for
   *         aNode, false otherwise.
   */
  hasCachedLivemarkInfo: function PC_hasCachedLivemarkInfo(aNode) {
    return this._cachedLivemarkInfoObjects.has(aNode);
  },

  /**
   * Returns the cached livemark info for a node, if set by cacheLivemarkInfo,
   * null otherwise.
   * @param aNode
   *        a places result node.
   * @return the mozILivemarkInfo object for aNode, if set, null otherwise.
   */
  getCachedLivemarkInfo: function PC_getCachedLivemarkInfo(aNode) {
    return this._cachedLivemarkInfoObjects.get(aNode, null);
  }
};

/**
 * Handles drag and drop operations for views. Note that this is view agnostic!
 * You should not use PlacesController._view within these methods, since
 * the view that the item(s) have been dropped on was not necessarily active.
 * Drop functions are passed the view that is being dropped on.
 */
var PlacesControllerDragHelper = {
  /**
   * DOM Element currently being dragged over
   */
  currentDropTarget: null,

  /**
   * Determines if the mouse is currently being dragged over a child node of
   * this menu. This is necessary so that the menu doesn't close while the
   * mouse is dragging over one of its submenus
   * @param   node
   *          The container node
   * @return true if the user is dragging over a node within the hierarchy of
   *         the container, false otherwise.
   */
  draggingOverChildNode: function PCDH_draggingOverChildNode(node) {
    let currentNode = this.currentDropTarget;
    while (currentNode) {
      if (currentNode == node)
        return true;
      currentNode = currentNode.parentNode;
    }
    return false;
  },

  /**
   * @return The current active drag session. Returns null if there is none.
   */
  getSession: function PCDH__getSession() {
    return this.dragService.getCurrentSession();
  },

  /**
   * Extract the first accepted flavor from a list of flavors.
   * @param aFlavors
   *        The flavors list of type DOMStringList.
   */
  getFirstValidFlavor: function PCDH_getFirstValidFlavor(aFlavors) {
    for (let i = 0; i < aFlavors.length; i++) {
      if (PlacesUIUtils.SUPPORTED_FLAVORS.includes(aFlavors[i]))
        return aFlavors[i];
    }

    // If no supported flavor is found, check if data includes text/plain
    // contents.  If so, request them as text/unicode, a conversion will happen
    // automatically.
    if (aFlavors.contains("text/plain")) {
        return PlacesUtils.TYPE_UNICODE;
    }

    return null;
  },

  /**
   * Determines whether or not the data currently being dragged can be dropped
   * on a places view.
   * @param ip
   *        The insertion point where the items should be dropped.
   */
  canDrop: function PCDH_canDrop(ip, dt) {
    let dropCount = dt.mozItemCount;

    // Check every dragged item.
    for (let i = 0; i < dropCount; i++) {
      let flavor = this.getFirstValidFlavor(dt.mozTypesAt(i));
      if (!flavor)
        return false;

      // Urls can be dropped on any insertionpoint.
      // XXXmano: remember that this method is called for each dragover event!
      // Thus we shouldn't use unwrapNodes here at all if possible.
      // I think it would be OK to accept bogus data here (e.g. text which was
      // somehow wrapped as TAB_DROP_TYPE, this is not in our control, and
      // will just case the actual drop to be a no-op), and only rule out valid
      // expected cases, which are either unsupported flavors, or items which
      // cannot be dropped in the current insertionpoint. The last case will
      // likely force us to use unwrapNodes for the private data types of
      // places.
      if (flavor == TAB_DROP_TYPE)
        continue;

      let data = dt.mozGetDataAt(flavor, i);
      let nodes;
      try {
        nodes = PlacesUtils.unwrapNodes(data, flavor);
      } catch (e) {
        return false;
      }

      for (let dragged of nodes) {
        // Only bookmarks and urls can be dropped into tag containers.
        if (ip.isTag &&
            dragged.type != PlacesUtils.TYPE_X_MOZ_URL &&
            (dragged.type != PlacesUtils.TYPE_X_MOZ_PLACE ||
             (dragged.uri && dragged.uri.startsWith("place:")) ))
          return false;

        // The following loop disallows the dropping of a folder on itself or
        // on any of its descendants.
        if (dragged.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER ||
            (dragged.uri && dragged.uri.startsWith("place:")) ) {
          let parentId = ip.itemId;
          while (parentId != PlacesUtils.placesRootId) {
            if (dragged.concreteId == parentId || dragged.id == parentId)
              return false;
            parentId = PlacesUtils.bookmarks.getFolderIdForItem(parentId);
          }
        }
      }
    }
    return true;
  },

  /**
   * Determines if an unwrapped node can be moved.
   *
   * @param unwrappedNode
   *        A node unwrapped by PlacesUtils.unwrapNodes().
   * @return True if the node can be moved, false otherwise.
   */
  canMoveUnwrappedNode(unwrappedNode) {
    if (unwrappedNode.id <= 0 || PlacesUtils.isRootItem(unwrappedNode.id)) {
      return false;
    }
    let parentId = unwrappedNode.parent;
    if (parentId <= 0 ||
        parentId == PlacesUtils.placesRootId ||
        parentId == PlacesUtils.tagsFolderId ||
        unwrappedNode.grandParentId == PlacesUtils.tagsFolderId) {
      return false;
    }
    // leftPaneFolderId and allBookmarksFolderId are lazy getters running
    // at least a synchronous DB query. Therefore we don't want to invoke
    // them first, especially because isCommandEnabled may be called way
    // before the left pane folder is even necessary.
    if (typeof Object.getOwnPropertyDescriptor(PlacesUIUtils, "leftPaneFolderId").get != "function" &&
        (parentId == PlacesUIUtils.leftPaneFolderId ||
          parentId == PlacesUIUtils.allBookmarksFolderId)) {
      return false;
    }
    return true;
  },

  /**
   * Determines if a node can be moved.
   *
   * @param   aNode
   *          A nsINavHistoryResultNode node.
   * @param   aView
   *          The view originating the request
   * @param   [optional] aDOMNode
   *          A XUL DOM node.
   * @return True if the node can be moved, false otherwise.
   */
  canMoveNode(aNode, aView, aDOMNode) {
    // Only bookmark items are movable.
    if (aNode.itemId == -1)
      return false;

    let parentNode = aNode.parent;
    if (!parentNode) {
      // Normally parentless places nodes can not be moved,
      // but simulated bookmarked URI nodes are special.
      return !!aDOMNode &&
             aDOMNode.hasAttribute("simulated-places-node") &&
             PlacesUtils.nodeIsBookmark(aNode);
    }

    // Once tags and bookmarked are divorced, the tag-query check should be
    // removed.
    return PlacesUtils.nodeIsFolder(parentNode) &&
           !PlacesUIUtils.isFolderReadOnly(parentNode, aView) &&
           !PlacesUtils.nodeIsTagQuery(parentNode);
  },

  /**
   * Handles the drop of one or more items onto a view.
   *
   * @param {Object} insertionPoint The insertion point where the items should
   *                                be dropped.
   * @param {Object} dt             The dataTransfer information for the drop.
   * @param {Object} view           The view or the tree element. This allows
   *                                batching to take place.
   */
  async onDrop(insertionPoint, dt, view) {
    let doCopy = ["copy", "link"].includes(dt.dropEffect);

    let transactions = [];
    let dropCount = dt.mozItemCount;
    let parentGuid = insertionPoint.guid;

    // Following flavors may contain duplicated data.
    let duplicable = new Map();
    duplicable.set(PlacesUtils.TYPE_UNICODE, new Set());
    duplicable.set(PlacesUtils.TYPE_X_MOZ_URL, new Set());

    // Collect all data from the DataTransfer before processing it, as the
    // DataTransfer is only valid during the synchronous handling of the `drop`
    // event handler callback.
    let dtItems = [];
    for (let i = 0; i < dropCount; ++i) {
      let flavor = this.getFirstValidFlavor(dt.mozTypesAt(i));
      if (!flavor)
        return;

      let data = dt.mozGetDataAt(flavor, i);
      if (duplicable.has(flavor)) {
        let handled = duplicable.get(flavor);
        if (handled.has(data))
          continue;
        handled.add(data);
      }
      dtItems.push({flavor, data});
    }

    if (PlacesUIUtils.useAsyncTransactions) {
      let nodes = [];
      // TODO: When sync transactions are removed, merge the for loop here into the
      // one above.
      for (let {flavor, data} of dtItems) {
        if (flavor != TAB_DROP_TYPE) {
          nodes = [...nodes, ...PlacesUtils.unwrapNodes(data, flavor)];
        } else if (data instanceof XULElement && data.localName == "tab" &&
                 data.ownerGlobal.isChromeWindow) {
          let uri = data.linkedBrowser.currentURI;
          let spec = uri ? uri.spec : "about:blank";
          nodes.push({
            uri: spec,
            title: data.label,
            type: PlacesUtils.TYPE_X_MOZ_URL
          });
        } else {
          throw new Error("bogus data was passed as a tab");
        }
      }

      await handleTransferItems(nodes, insertionPoint, doCopy, view);
    } else {
      for (let {flavor, data} of dtItems) {
        let nodes;
        if (flavor != TAB_DROP_TYPE) {
          nodes = PlacesUtils.unwrapNodes(data, flavor);
        } else if (data instanceof XULElement && data.localName == "tab" &&
                 data.ownerGlobal.isChromeWindow) {
          let uri = data.linkedBrowser.currentURI;
          let spec = uri ? uri.spec : "about:blank";
          nodes = [{ uri: spec,
                     title: data.label,
                     type: PlacesUtils.TYPE_X_MOZ_URL}];
        } else {
          throw new Error("bogus data was passed as a tab");
        }

        let movedCount = 0;
        for (let unwrapped of nodes) {
          let index = await insertionPoint.getIndex();

          if (index != -1 && unwrapped.itemGuid) {
            // Note: we use the parent from the existing bookmark as the sidebar
            // gives us an unwrapped.parent that is actually a query and not the real
            // parent.
            let existingBookmark = await PlacesUtils.bookmarks.fetch(unwrapped.itemGuid);

            // If we're dropping on the same folder, then we may need to adjust
            // the index to insert at the correct place.
            if (existingBookmark && parentGuid == existingBookmark.parentGuid) {
              // Sync Transactions. Adjust insertion index to prevent reversal
              // of dragged items. When you drag multiple elts upward: need to
              // increment index or each successive elt will be inserted at the
              // same index, each above the previous.
              if (index < existingBookmark.index) { // eslint-disable-line no-lonely-if
                index += movedCount++;
              }
            }
          }

          // If dragging over a tag container we should tag the item.
          // eslint-disable-next-line no-lonely-if
          if (insertionPoint.isTag) {
            let uri = NetUtil.newURI(unwrapped.uri);
            let tagItemId = insertionPoint.itemId;
            transactions.push(new PlacesTagURITransaction(uri, [tagItemId]));
          } else {
            // If this is not a copy, check for safety that we can move the
            // source, otherwise report an error and fallback to a copy.
            if (!doCopy && !PlacesControllerDragHelper.canMoveUnwrappedNode(unwrapped)) {
              Components.utils.reportError("Tried to move an unmovable Places " +
                                           "node, reverting to a copy operation.");
              doCopy = true;
            }
            transactions.push(PlacesUIUtils.makeTransaction(unwrapped,
                                flavor, insertionPoint.itemId,
                                index, doCopy));
          }
        }

        // Check if we actually have something to add, if we don't it probably wasn't
        // valid, or it was moving to the same location, so just ignore it.
        if (!transactions.length) {
          return;
        }

        let txn = new PlacesAggregatedTransaction("DropItems", transactions);
        PlacesUtils.transactionManager.doTransaction(txn);
      }
    }
  },

  /**
   * Checks if we can insert into a container.
   * @param   aContainer
   *          The container were we are want to drop
   * @param   aView
   *          The view generating the request
   */
  disallowInsertion(aContainer, aView) {
    NS_ASSERT(aContainer, "empty container");
    // Allow dropping into Tag containers and editable folders.
    return !PlacesUtils.nodeIsTagQuery(aContainer) &&
           (!PlacesUtils.nodeIsFolder(aContainer) ||
            PlacesUIUtils.isFolderReadOnly(aContainer, aView));
  }
};


XPCOMUtils.defineLazyServiceGetter(PlacesControllerDragHelper, "dragService",
                                   "@mozilla.org/widget/dragservice;1",
                                   "nsIDragService");

function goUpdatePlacesCommands() {
  // Get the controller for one of the places commands.
  var placesController = doGetPlacesControllerForCommand("placesCmd_open");
  function updatePlacesCommand(aCommand) {
    goSetCommandEnabled(aCommand, placesController &&
                                  placesController.isCommandEnabled(aCommand));
  }

  updatePlacesCommand("placesCmd_open");
  updatePlacesCommand("placesCmd_open:window");
  updatePlacesCommand("placesCmd_open:privatewindow");
  updatePlacesCommand("placesCmd_open:tab");
  updatePlacesCommand("placesCmd_new:folder");
  updatePlacesCommand("placesCmd_new:bookmark");
  updatePlacesCommand("placesCmd_new:separator");
  updatePlacesCommand("placesCmd_show:info");
  updatePlacesCommand("placesCmd_moveBookmarks");
  updatePlacesCommand("placesCmd_reload");
  updatePlacesCommand("placesCmd_sortBy:name");
  updatePlacesCommand("placesCmd_cut");
  updatePlacesCommand("placesCmd_copy");
  updatePlacesCommand("placesCmd_paste");
  updatePlacesCommand("placesCmd_delete");
}

function doGetPlacesControllerForCommand(aCommand) {
  // A context menu may be built for non-focusable views.  Thus, we first try
  // to look for a view associated with document.popupNode
  let popupNode;
  try {
    popupNode = document.popupNode;
  } catch (e) {
    // The document went away (bug 797307).
    return null;
  }
  if (popupNode) {
    let view = PlacesUIUtils.getViewForNode(popupNode);
    if (view && view._contextMenuShown)
      return view.controllers.getControllerForCommand(aCommand);
  }

  // When we're not building a context menu, only focusable views
  // are possible.  Thus, we can safely use the command dispatcher.
  let controller = top.document.commandDispatcher
                      .getControllerForCommand(aCommand);
  if (controller)
    return controller;

  return null;
}

function goDoPlacesCommand(aCommand) {
  let controller = doGetPlacesControllerForCommand(aCommand);
  if (controller && controller.isCommandEnabled(aCommand))
    controller.doCommand(aCommand);
}

/**
 * This gets the most appropriate item for using for batching. In the case of multiple
 * views being related, the method returns the most expensive result to batch.
 * For example, if it detects the left-hand library pane, then it will look for
 * and return the reference to the right-hand pane.
 *
 * @param {Object} viewOrElement The item to check.
 * @return {Object} Will return the best result node to batch, or null
 *                  if one could not be found.
 */
function getResultForBatching(viewOrElement) {
  if (viewOrElement && viewOrElement instanceof Ci.nsIDOMElement &&
      viewOrElement.id === "placesList") {
    // Note: fall back to the existing item if we can't find the right-hane pane.
    viewOrElement = document.getElementById("placeContent") || viewOrElement;
  }

  if (viewOrElement && viewOrElement.result) {
    return viewOrElement.result;
  }

  return null;
}

/**
 * Processes a set of transfer items that have been dropped or pasted.
 * Batching will be applied where necessary.
 *
 * @param {Array} items A list of unwrapped nodes to process.
 * @param {Object} insertionPoint The requested point for insertion.
 * @param {Boolean} doCopy Set to true to copy the items, false will move them
 *                         if possible.
 * @return {Array} Returns an empty array when the insertion point is a tag, else
 *                 returns an array of copied or moved guids.
 */
async function handleTransferItems(items, insertionPoint, doCopy, view) {
  let transactions;
  if (insertionPoint.isTag) {
    let urls = items.filter(item => "uri" in item).map(item => item.uri);
    transactions = [PlacesTransactions.Tag({ urls, tag: insertionPoint.tagName })];
  } else {
    let insertionIndex = await insertionPoint.getIndex();
    transactions = await getTransactionsForTransferItems(
      items, insertionIndex, insertionPoint.guid, doCopy);
  }

  // Check if we actually have something to add, if we don't it probably wasn't
  // valid, or it was moving to the same location, so just ignore it.
  if (!transactions.length) {
    return [];
  }

  let guidsToSelect = [];
  let resultForBatching = getResultForBatching(view);

  // If we're inserting into a tag, we don't get the guid, so we'll just
  // pass the transactions direct to the batch function.
  let batchingItem = transactions;
  if (!insertionPoint.isTag) {
    // If we're not a tag, then we need to get the ids of the items to select.
    batchingItem = async () => {
      for (let transaction of transactions) {
        let guid = await transaction.transact();
        if (guid) {
          guidsToSelect.push(guid);
        }
      }
    };
  }

  await PlacesUIUtils.batchUpdatesForNode(resultForBatching,
    transactions.length, async () => {
      await PlacesTransactions.batch(batchingItem);
    });

  return guidsToSelect;
}

/**
 * Processes a set of transfer items and returns transactions to insert or
 * move them.
 *
 * @param {Array} items A list of unwrapped nodes to get transactions for.
 * @param {Integer} insertionIndex The requested index for insertion.
 * @param {String} insertionParentGuid The guid of the parent folder to insert
 *                                     or move the items to.
 * @param {Boolean} doCopy Set to true to copy the items, false will move them
 *                         if possible.
 * @return {Array} Returns an array of created PlacesTransactions.
 */
async function getTransactionsForTransferItems(items, insertionIndex,
                                               insertionParentGuid, doCopy) {
  let transactions = [];
  let index = insertionIndex;

  for (let item of items) {
    if (index != -1 && item.itemGuid) {
      // Note: we use the parent from the existing bookmark as the sidebar
      // gives us an unwrapped.parent that is actually a query and not the real
      // parent.
      let existingBookmark = await PlacesUtils.bookmarks.fetch(item.itemGuid);

      // If we're dropping on the same folder, then we may need to adjust
      // the index to insert at the correct place.
      if (existingBookmark && insertionParentGuid == existingBookmark.parentGuid) {
        if (index > existingBookmark.index) {
          // If we're dragging down, we need to go one lower to insert at
          // the real point as moving the element changes the index of
          // everything below by 1.
          index--;
        } else if (index == existingBookmark.index) {
          // This isn't moving so we skip it.
          continue;
        }
      }
    }

    // If this is not a copy, check for safety that we can move the
    // source, otherwise report an error and fallback to a copy.
    if (!doCopy && !PlacesControllerDragHelper.canMoveUnwrappedNode(item)) {
      Components.utils.reportError("Tried to move an unmovable Places " +
                                   "node, reverting to a copy operation.");
      doCopy = true;
    }
    transactions.push(
      PlacesUIUtils.getTransactionForData(item,
                                          insertionParentGuid,
                                          index,
                                          doCopy));

    if (index != -1 && item.itemGuid) {
      index++;
    }
  }
  return transactions;
}
