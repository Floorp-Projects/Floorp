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

const NHRVO = Ci.nsINavHistoryResultViewObserver;

// XXXmano: we should move most/all of these constants to PlacesUtils
const ORGANIZER_ROOT_BOOKMARKS = "place:folder=2&excludeItems=1&queryType=1";
const ORGANIZER_SUBSCRIPTIONS_QUERY = "place:annotation=livemark%2FfeedURI";

// No change to the view, preserve current selection
const RELOAD_ACTION_NOTHING = 0;
// Inserting items new to the view, select the inserted rows
const RELOAD_ACTION_INSERT = 1;
// Removing items from the view, select the first item after the last selected
const RELOAD_ACTION_REMOVE = 2;
// Moving items within a view, don't treat the dropped items as additional 
// rows.
const RELOAD_ACTION_MOVE = 3;

/**
 * Represents an insertion point within a container where we can insert
 * items. 
 * @param   aItemId
 *          The identifier of the parent container
 * @param   aIndex
 *          The index within the container where we should insert
 * @param   aOrientation
 *          The orientation of the insertion. NOTE: the adjustments to the
 *          insertion point to accommodate the orientation should be done by
 *          the person who constructs the IP, not the user. The orientation
 *          is provided for informational purposes only!
 * @constructor
 */
function InsertionPoint(aItemId, aIndex, aOrientation) {
  this.itemId = aItemId;
  this.index = aIndex;
  this.orientation = aOrientation;
}
InsertionPoint.prototype.toString = function IP_toString() {
  return "[object InsertionPoint(folder:" + this.itemId + ",index:" + this.index + ",orientation:" + this.orientation + ")]";
};

/**
 * Places Controller
 */

function PlacesController(aView) {
  this._view = aView;
}

PlacesController.prototype = {
  /**
   * The places view.
   */
  _view: null,

  isCommandEnabled: function PC_isCommandEnabled(aCommand) {
    switch (aCommand) {
    case "cmd_undo":
      return PlacesUtils.tm.numberOfUndoItems > 0;
    case "cmd_redo":
      return PlacesUtils.tm.numberOfRedoItems > 0;
    case "cmd_cut":
    case "cmd_delete":
      return this._hasRemovableSelection(false);
    case "placesCmd_moveBookmarks":
      return this._hasRemovableSelection(true);
    case "cmd_copy":
      return this._view.hasSelection;
    case "cmd_paste":
      return this._canInsert() && this._isClipboardDataPasteable();
    case "cmd_selectAll":
      if (this._view.selType != "single") {
        var result = this._view.getResult();
        if (result) {
          var container = asContainer(result.root);
          if (container.childCount > 0);
            return true;
        }
      }
      return false;
    case "placesCmd_open":
    case "placesCmd_open:window":
    case "placesCmd_open:tab":
      return this._view.selectedURINode;
    case "placesCmd_new:folder":
    case "placesCmd_new:livemark":
      return this._canInsert() &&
             this._view.peerDropTypes
                 .indexOf(PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) != -1;
    case "placesCmd_new:bookmark":
      return this._canInsert() &&
             this._view.peerDropTypes.indexOf(PlacesUtils.TYPE_X_MOZ_URL) != -1;
    case "placesCmd_new:separator":
      return this._canInsert() &&
             this._view.peerDropTypes
                 .indexOf(PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR) != -1 &&
             !asQuery(this._view.getResult().root).queryOptions.excludeItems &&
             this._view.getResult().sortingMode ==
                 Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
    case "placesCmd_show:info":
      if (this._view.hasSingleSelection) {
        var selectedNode = this._view.selectedNode;
        if (PlacesUtils.nodeIsFolder(selectedNode) ||
            (PlacesUtils.nodeIsBookmark(selectedNode) &&
            !PlacesUtils.nodeIsLivemarkItem(selectedNode)))
          return true;
      }
      return false;
    case "placesCmd_reloadMicrosummary":
      if (this._view.hasSingleSelection) {
        var selectedNode = this._view.selectedNode;
        if (PlacesUtils.nodeIsBookmark(selectedNode)) {
          var mss = PlacesUtils.microsummaries;
          if (mss.hasMicrosummary(selectedNode.itemId))
            return true;
        }
      }
      return false;
    case "placesCmd_reload":
      if (this._view.hasSingleSelection) {
        var selectedNode = this._view.selectedNode;

        // Livemark containers
        if (PlacesUtils.nodeIsLivemarkContainer(selectedNode))
          return true;
      }
      return false;
    case "placesCmd_sortBy:name":
      var selectedNode = this._view.selectedNode;
      return selectedNode &&
             PlacesUtils.nodeIsFolder(selectedNode) &&
             !PlacesUtils.nodeIsReadOnly(selectedNode) &&
             this._view.getResult().sortingMode ==
                 Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
    default:
      return false;
    }
  },

  supportsCommand: function PC_supportsCommand(aCommand) {
    //LOG("supportsCommand: " + command);
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

  doCommand: function PC_doCommand(aCommand) {
    switch (aCommand) {
    case "cmd_undo":
      PlacesUtils.tm.undoTransaction();
      break;
    case "cmd_redo":
      PlacesUtils.tm.redoTransaction();
      break;
    case "cmd_cut":
      this.cut();
      break;
    case "cmd_copy":
      this.copy();
      break;
    case "cmd_paste":
      this.paste();
      break;
    case "cmd_delete":
      this.remove("Remove Selection");
      break;
    case "cmd_selectAll":
      this.selectAll();
      break;
    case "placesCmd_open":
      this.openSelectedNodeIn("current");
      break;
    case "placesCmd_open:window":
      this.openSelectedNodeIn("window");
      break;
    case "placesCmd_open:tab":
      this.openSelectedNodeIn("tab");
      break;
    case "placesCmd_new:folder":
      this.newItem("folder");
      break;
    case "placesCmd_new:bookmark":
      this.newItem("bookmark");
      break;
    case "placesCmd_new:livemark":
      this.newItem("livemark");
      break;
    case "placesCmd_new:separator":
      this.newSeparator();
      break;
    case "placesCmd_show:info":
      this.showBookmarkPropertiesForSelection();
      break;
    case "placesCmd_moveBookmarks":
      this.moveSelectedBookmarks();
      break;
    case "placesCmd_reload":
      this.reloadSelectedLivemark();
      break;
    case "placesCmd_reloadMicrosummary":
      this.reloadSelectedMicrosummary();
      break;
    case "placesCmd_sortBy:name":
      this.sortFolderByName();
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
   * @param aIsMoveCommand
   *        True if thecommand for which this method is called only moves the
   *        selected items to another container, false otherwise.
   * @returns true if the there's a selection which has no nodes that cannot be removed,
   *          false otherwise.
   */
  _hasRemovableSelection: function PC__hasRemovableSelection(aIsMoveCommand) {
    if (!this._view.hasSelection)
      return false;

    var nodes = this._view.getSelectionNodes();
    var root = this._view.getResultNode();

    var btFolderId = PlacesUtils.toolbarFolderId;
    for (var i = 0; i < nodes.length; ++i) {
      // Disallow removing the view's root node 
      if (nodes[i] == root)
        return false;

      // Disallow removing the toolbar folder
      if (!aIsMoveCommand &&
          PlacesUtils.nodeIsFolder(nodes[i]) && nodes[i].itemId == btFolderId)
        return false;

      // We don't call nodeIsReadOnly here, because nodeIsReadOnly means that
      // a node has children that cannot be edited, reordered or removed. Here,
      // we don't care if a node's children can't be reordered or edited, just
      // that they're removable. All history results have removable children
      // (based on the principle that any URL in the history table should be
      // removable), but some special bookmark folders may have non-removable
      // children, e.g. live bookmark folder children. It doesn't make sense
      // to delete a child of a live bookmark folder, since when the folder
      // refreshes, the child will return.
      var parent = nodes[i].parent || root;
      if (PlacesUtils.isReadonlyFolder(parent))
        return false;
    }
    return true;
  },

  /**
   * Determines whether or not nodes can be inserted relative to the selection.
   */
  _canInsert: function PC__canInsert() {
    return this._view.insertionPoint != null;
  },

  /**
   * Determines whether or not the root node for the view is selected
   */
  rootNodeIsSelected: function PC_rootNodeIsSelected() {
    if (this._view.hasSelection) {
      var nodes = this._view.getSelectionNodes();
      var root = this._view.getResultNode();
      for (var i = 0; i < nodes.length; ++i) {
        if (nodes[i] == root)
          return true;      
      }
    }
    return false;
  },

  /**
   * Looks at the data on the clipboard to see if it is paste-able. 
   * Paste-able data is:
   *   - in a format that the view can receive
   * @returns true if: - clipboard data is of a TYPE_X_MOZ_PLACE_* flavor,
                       - clipboard data is of type TEXT_UNICODE and
                         is a valid URI.
   */
  _isClipboardDataPasteable: function PC__isClipboardDataPasteable() {
    // if the clipboard contains TYPE_X_MOZ_PLACE_* data, it is definitely
    // pasteable, with no need to unwrap all the nodes.

    var flavors = PlacesUtils.placesFlavors;
    var clipboard = PlacesUtils.clipboard;
    var hasPlacesData =
      clipboard.hasDataMatchingFlavors(flavors, flavors.length,
                                       Ci.nsIClipboard.kGlobalClipboard);
    if (hasPlacesData)
      return this._view.insertionPoint != null;

    // if the clipboard doesn't have TYPE_X_MOZ_PLACE_* data, we also allow
    // pasting of valid "text/unicode" and "text/x-moz-url" data
    var xferable = Cc["@mozilla.org/widget/transferable;1"].
                   createInstance(Ci.nsITransferable);

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
      var unwrappedNodes = PlacesUtils.unwrapNodes(data, type.value);
      return this._view.insertionPoint != null;
    }
    catch (e) {
      // getAnyTransferData or unwrapNodes failed
      return false;
    }
  },

  /** 
   * Gathers information about the selected nodes according to the following
   * rules:
   *    "link"              node is a URI
   *    "bookmark"          node is a bookamrk
   *    "livemarkChild"     node is a child of a livemark
   *    "folder"            node is a folder
   *    "query"             node is a query
   *    "dynamiccontainer"  node is a dynamic container
   *    "separator"         node is a separator line
   *    "host"              node is a host
   *    "mutable"           node can have items inserted or reordered
   *    
   * @returns an array of objects corresponding the selected nodes. Each
   *          object has each of the properties above set if its corresponding
   *          node matches the rule. In addition, the annotations names for each 
   *          node are set on its corresponding object as properties.
   * Notes:
   *   1) This can be slow, so don't call it anywhere performance critical!
   *   2) A single-object array corresponding the root node is returned if
   *      there's no selection.
   */
  _buildSelectionMetadata: function PC__buildSelectionMetadata() {
    var metadata = [];
    var nodes = [];
    var root = this._view.getResult().root;
    if (this._view.hasSelection)
      nodes = this._view.getSelectionNodes();
    else // See the second note above
      nodes = [root];

    for (var i=0; i < nodes.length; i++) {
      var nodeData = {};
      var node = nodes[i];
      var nodeType = node.type;
      var uri = null;

      // We don't use the nodeIs* methods here to avoid going through the type
      // property way too often
      switch(nodeType) {
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY:
          nodeData["query"] = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_DYNAMIC_CONTAINER:
          nodeData["dynamiccontainer"] = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER:
          nodeData["folder"] = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_HOST:
          nodeData["host"] = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR:
          nodeData["separator"] = true;
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_URI:
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_VISIT:
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_FULL_VISIT:
          nodeData["link"] = true;
          uri = PlacesUtils._uri(node.uri);
          if (PlacesUtils.nodeIsBookmark(node)) {
            nodeData["bookmark"] = true;
            var mss = PlacesUtils.microsummaries;
            if (mss.hasMicrosummary(node.itemId))
              nodeData["microsummary"] = true;
            else if (node.parent &&
                     PlacesUtils.nodeIsLivemarkContainer(node.parent))
              nodeData["livemarkChild"] = true;
          }
          break;
        case Ci.nsINavHistoryResultNode.RESULT_TYPE_DAY:
          nodeData["day"] = true;
      }

      // Mutability is whether or not a container can have selected items
      // inserted or reordered. It does _not_ dictate whether or not the 
      // container can have items removed from it, since some containers that
      // aren't  reorderable can have items removed from them, e.g. a history
      // list. 
      if (!PlacesUtils.nodeIsReadOnly(node) &&
          !PlacesUtils.isReadonlyFolder(node.parent || root))
        nodeData["mutable"] = true;

      // annotations
      if (uri) {
        var names = PlacesUtils.annotations.getPageAnnotationNames(uri, {});
        for (var j = 0; j < names.length; ++j)
          nodeData[names[j]] = true;
      }

      // For items also include the item-specific annotations
      if (node.itemId != -1) {
        names = PlacesUtils.annotations
                           .getItemAnnotationNames(node.itemId, {});
        for (j = 0; j < names.length; ++j)
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
   * @returns true if the conditions (see buildContextMenu) are satisfied
   *          and the item can be displayed, false otherwise. 
   */
  _shouldShowMenuItem: function PC__shouldShowMenuItem(aMenuItem, aMetaData) {
    var selectiontype = aMenuItem.getAttribute("selectiontype");
    if (selectiontype == "multiple" && aMetaData.length == 1)
      return false;
    if (selectiontype == "single" && aMetaData.length != 1)
      return false;

    var forceHideRules = aMenuItem.getAttribute("forcehideselection").split("|");
    for (var i = 0; i < aMetaData.length; ++i) {
      for (var j=0; j < forceHideRules.length; ++j) {
        if (forceHideRules[j] in aMetaData[i])
          return false;
      }
    }

    if (aMenuItem.hasAttribute("selection")) {
      var showRules = aMenuItem.getAttribute("selection").split("|");
      var anyMatched = false;
      function metaDataNodeMatches(metaDataNode, rules) {
        for (var i=0; i < rules.length; i++) {
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
    }

    return !aMenuItem.hidden;
  },

  /**
   * Detects information (meta-data rules) about the current selection in the
   * view (see _buildSelectionMetadata) and sets the visibility state for each
   * of the menu-items in the given popup with the following rules applied:
   *  1) The "selectiontype" attribute may be set on a menu-item to "single"
   *     if the menu-item should be visible only if there is a single node
   *     selected, or to "multiple" if the menu-item should be visible only if
   *     multiple nodes are selected. If the attribute is not set or if it is
   *     set to an invalid value, the menu-item may be visible for both types of
   *     selection.
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
   *  5) The visibility state of a menu-item is unchanged if none of these
   *     attribute are set.
   *  6) These attributes should not be set on separators for which the
   *     visibility state is "auto-detected."
   * @param   aPopup
   *          The menupopup to build children into.
   * @return true if at least one item is visible, false otherwise.
   */
  buildContextMenu: function PC_buildContextMenu(aPopup) {
    var metadata = this._buildSelectionMetadata();

    var separator = null;
    var visibleItemsBeforeSep = false;
    var anyVisible = false;
    for (var i = 0; i < aPopup.childNodes.length; ++i) {
      var item = aPopup.childNodes[i];
      if (item.localName != "menuseparator") {
        item.hidden = !this._shouldShowMenuItem(item, metadata);
        if (!item.hidden) {
          visibleItemsBeforeSep = true;
          anyVisible = true;

          // Show the separator above the menu-item if any
          if (separator) {
            separator.hidden = false;
            separator = null;
          }
        }
      }
      else { // menuseparator
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
    if (anyVisible) {
      var openContainerInTabsItem = document.getElementById("placesContext_openContainer:tabs");
      if (!openContainerInTabsItem.hidden && this._view.selectedNode &&
          PlacesUtils.nodeIsContainer(this._view.selectedNode)) {
        openContainerInTabsItem.disabled =
          PlacesUtils.getURLsForContainerNode(this._view.selectedNode)
                     .length == 0;
      }
      else {
        // see selectiontype rule in the overlay
        var openLinksInTabsItem = document.getElementById("placesContext_openLinks:tabs");
        openLinksInTabsItem.disabled = openLinksInTabsItem.hidden;
      }
    }

    return anyVisible;
  },

  /**
   * Select all links in the current view. 
   */
  selectAll: function PC_selectAll() {
    this._view.selectAll();
  },

  /**
   * Loads the selected node's URL in the appropriate tab or window or as a web
   * panel given the user's preference specified by modifier keys tracked by a
   * DOM mouse/key event.
   * @param   aEvent
   *          The DOM mouse/key event with modifier keys set that track the
   *          user's preferred destination window or tab.
   */
  openSelectedNodeWithEvent: function PC_openSelectedNodeWithEvent(aEvent) {
    this.openSelectedNodeIn(whereToOpenLink(aEvent));
  },

  /**
   * Loads the selected node's URL in the appropriate tab or window or as a
   * web panel.
   * see also openUILinkIn
   */
  openSelectedNodeIn: function PC_openSelectedNodeIn(aWhere) {
    var node = this._view.selectedURINode;
    if (node && PlacesUtils.checkURLSecurity(node)) {
       // Check whether the node is a bookmark which should be opened as
       // a web panel
      if (aWhere == "current" && PlacesUtils.nodeIsBookmark(node)) {
        if (PlacesUtils.annotations
                       .itemHasAnnotation(node.itemId, LOAD_IN_SIDEBAR_ANNO)) {
          var w = getTopWin();
          if (w) {
            w.openWebPanel(node.title, node.uri);
            return;
          }
        }
      }
      openUILinkIn(node.uri, aWhere);
    }
  },

  /**
   * Opens the bookmark properties for the selected URI Node.
   */
  showBookmarkPropertiesForSelection: 
  function PC_showBookmarkPropertiesForSelection() {
    var node = this._view.selectedNode;
    if (!node)
      return;

    if (PlacesUtils.nodeIsFolder(node))
      PlacesUtils.showFolderProperties(node.itemId);
    else if (PlacesUtils.nodeIsBookmark(node))
      PlacesUtils.showBookmarkProperties(node.itemId);
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
    if (this._view.hasSingleSelection) {
      var selectedNode = this._view.selectedNode;
      if (PlacesUtils.nodeIsLivemarkContainer(selectedNode))
        PlacesUtils.livemarks.reloadLivemarkFolder(selectedNode.itemId);
    }
  },

  /**
   * Reload the microsummary associated with the selection
   */
  reloadSelectedMicrosummary: function PC_reloadSelectedMicrosummary() {
    var selectedNode = this._view.selectedNode;
    var mss = PlacesUtils.microsummaries;
    if (mss.hasMicrosummary(selectedNode.itemId))
      mss.refreshMicrosummary(selectedNode.itemId);
  },

  /**
   * Gives the user a chance to cancel loading lots of tabs at once
   */
  _confirmOpenTabs: function(numTabsToOpen) {
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
        var strings = document.getElementById("placeBundle");
        const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
        var brandShortName = Cc["@mozilla.org/intl/stringbundle;1"].
                             getService(Ci.nsIStringBundleService).
                             createBundle(BRANDING_BUNDLE_URI).
                             GetStringFromName("brandShortName");
       
        var buttonPressed = promptService.confirmEx(window,
          PlacesUtils.getString("tabs.openWarningTitle"),
          PlacesUtils.getFormattedString(messageKey, 
            [numTabsToOpen, brandShortName]),
          (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0)
          + (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
          PlacesUtils.getString(openKey),
          null, null,
          PlacesUtils.getFormattedString("tabs.openWarningPromptMeBranded",
            [brandShortName]),
          warnOnOpen);

         reallyOpen = (buttonPressed == 0);
         // don't set the pref unless they press OK and it's false
         if (reallyOpen && !warnOnOpen.value)
           pref.setBoolPref(kWarnOnOpenPref, false);
      }
    }
    return reallyOpen;
  },

  /**
   * Opens the links in the selected folder, or the selected links in new tabs. 
   */
  openSelectionInTabs: function PC_openLinksInTabs(aEvent) {
    var node = this._view.selectedNode;
    if (this._view.hasSingleSelection && PlacesUtils.nodeIsContainer(node))
      PlacesUtils.openContainerNodeInTabs(this._view.selectedNode, aEvent);
    else
      PlacesUtils.openURINodesInTabs(this._view.getSelectionNodes(), aEvent);
  },

  /**
   * Shows the Add Bookmark UI for the current insertion point.
   *
   * @param aType
   *        the type of the new item (bookmark/livemark/folder)
   */
  newItem: function PC_newItem(aType) {
    var ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    this._view.saveSelection(this._view.SAVE_SELECTION_INSERT);
    
    var performed = false;
    if (aType == "bookmark")
      performed = PlacesUtils.showAddBookmarkUI(null, null, null, ip);
    else if (aType == "livemark")
      performed = PlacesUtils.showAddLivemarkUI(null, null, null, null, ip);
    else // folder
      performed = PlacesUtils.showAddFolderUI(null, ip);

    if (performed)
      this._view.restoreSelection();
  },


  /**
   * Create a new Bookmark folder somewhere. Prompts the user for the name
   * of the folder. 
   */
  newFolder: function PC_newFolder() {
    var ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    this._view.saveSelection(this._view.SAVE_SELECTION_INSERT);
    if (PlacesUtils.showAddFolderUI(null, ip))
      this._view.restoreSelection();
  },

  /**
   * Create a new Bookmark separator somewhere.
   */
  newSeparator: function PC_newSeparator() {
    var ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    var txn = PlacesUtils.ptm.createSeparator(ip.itemId, ip.index);
    PlacesUtils.ptm.commitTransaction(txn);
  },

  /**
   * Opens a dialog for moving the selected nodes.
   */
  moveSelectedBookmarks: function PC_moveBookmarks() {
    window.openDialog("chrome://browser/content/places/moveBookmarks.xul",
                      "", "chrome, modal",
                      this._view.getSelectionNodes(), PlacesUtils.tm);
  },

  /**
   * Sort the selected folder by name
   */
  sortFolderByName: function PC_sortFolderByName() {
    var selectedNode = this._view.selectedNode;
    var txn = PlacesUtils.ptm.sortFolderByName(selectedNode.itemId,
                                               selectedNode.bookmarkIndex);
    PlacesUtils.ptm.commitTransaction(txn);
  },

  /**
   * Walk the list of folders we're removing in this delete operation, and
   * see if the selected node specified is already implicitly being removed 
   * because it is a child of that folder. 
   * @param   node
   *          Node to check for containment. 
   * @param   pastFolders
   *          List of folders the calling function has already traversed
   * @returns true if the node should be skipped, false otherwise. 
   */
  _shouldSkipNode: function PC_shouldSkipNode(node, pastFolders) {
    /**
     * Determines if a node is contained by another node within a resultset. 
     * @param   node
     *          The node to check for containment for
     * @param   parent
     *          The parent container to check for containment in
     * @returns true if node is a member of parent's children, false otherwise.
     */
    function isContainedBy(node, parent) {
      var cursor = node.parent;
      while (cursor) {
        if (cursor == parent)
          return true;
        cursor = cursor.parent;
      }
      return false;
    }
  
      for (var j = 0; j < pastFolders.length; ++j) {
        if (isContainedBy(node, pastFolders[j]))
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
   */
  _removeRange: function PC__removeRange(range, transactions) {
    NS_ASSERT(transactions instanceof Array, "Must pass a transactions array");

    var removedFolders = [];

    for (var i = 0; i < range.length; ++i) {
      var node = range[i];
      if (this._shouldSkipNode(node, removedFolders))
        continue;

      if (PlacesUtils.nodeIsFolder(node))
        removedFolders.push(node);

      transactions.push(PlacesUtils.ptm.removeItem(node.itemId));
    }
  },

  /**
   * Removes the set of selected ranges from bookmarks.
   * @param   txnName
   *          See |remove|.
   */
  _removeRowsFromBookmarks: function PC__removeRowsFromBookmarks(txnName) {
    var ranges = this._view.getRemovableSelectionRanges();
    var transactions = [];
    // Delete the selected rows. Do this by walking the selection backward, so
    // that when undo is performed they are re-inserted in the correct order.
    for (var i = ranges.length - 1; i >= 0 ; --i)
      this._removeRange(ranges[i], transactions);
    if (transactions.length > 0) {
      var txn = PlacesUtils.ptm.aggregateTransactions(txnName, transactions);
      PlacesUtils.ptm.commitTransaction(txn);
    }
  },

  /**
   * Removes the set of selected ranges from history. 
   */
  _removeRowsFromHistory: function PC__removeRowsFromHistory() {
    // Other containers are history queries, just delete from history
    // history deletes are not undoable.
    var nodes = this._view.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      var bhist = PlacesUtils.history.QueryInterface(Ci.nsIBrowserHistory);
      if (PlacesUtils.nodeIsHost(node))
        bhist.removePagesFromHost(node.title, true);
      else if (PlacesUtils.nodeIsURI(node))
        bhist.removePage(PlacesUtils._uri(node.uri));
    }
  },

  /**
   * Removes the selection
   * @param   aTxnName
   *          A name for the transaction if this is being performed
   *          as part of another operation.
   */
  remove: function PC_remove(aTxnName) {
    NS_ASSERT(aTxnName !== undefined, "Must supply Transaction Name");
    this._view.saveSelection(this._view.SAVE_SELECTION_REMOVE);

    var root = this._view.getResult().root;

    if (PlacesUtils.nodeIsFolder(root)) 
      this._removeRowsFromBookmarks(aTxnName);
    else if (PlacesUtils.nodeIsQuery(root)) {
      var queryType = asQuery(root).queryOptions.queryType;
      if (queryType == Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS)
        this._removeRowsFromBookmarks(aTxnName);
      else if (queryType == Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY)
        this._removeRowsFromHistory();
      else
        NS_ASSERT(false, "implement support for QUERY_TYPE_UNIFIED");
    }
    else
      NS_ASSERT(false, "unexpected root");
      
    this._view.restoreSelection();
  },

  /**
   * Get a TransferDataSet containing the content of the selection that can be
   * dropped elsewhere. 
   * @param   dragAction
   *          The action to happen when dragging, i.e. copy
   * @returns A TransferDataSet object that can be dragged and dropped 
   *          elsewhere.
   */
  getTransferData: function PC_getTransferData(dragAction) {
    var result = this._view.getResult();
    var oldViewer = result.viewer;
    try {
      result.viewer = null;
      var nodes = null;
      if (dragAction == Ci.nsIDragService.DRAGDROP_ACTION_COPY)
        nodes = this._view.getCopyableSelection();
      else
        nodes = this._view.getDragableSelection();
      var dataSet = new TransferDataSet();
      for (var i = 0; i < nodes.length; ++i) {
        var node = nodes[i];

        var data = new TransferData();
        function addData(type, overrideURI) {
          data.addDataForFlavour(type, PlacesUtils._wrapString(
                                 PlacesUtils.wrapNode(node, type, overrideURI)));
        }

        function addURIData(overrideURI) {
          addData(PlacesUtils.TYPE_X_MOZ_URL, overrideURI);
          addData(PlacesUtils.TYPE_UNICODE, overrideURI);
          addData(PlacesUtils.TYPE_HTML, overrideURI);
        }

        // This order is _important_! It controls how this and other 
        // applications select data to be inserted based on type.
        addData(PlacesUtils.TYPE_X_MOZ_PLACE);
      
        var uri;
      
        // Allow dropping the feed uri of live-bookmark folders
        if (PlacesUtils.nodeIsLivemarkContainer(node))
          uri = PlacesUtils.livemarks.getFeedURI(node.itemId).spec;
      
        addURIData(uri);
        dataSet.push(data);
      }
    }
    finally {
      if (oldViewer)
        result.viewer = oldViewer;
    }
    return dataSet;
  },

  /**
   * Copy Bookmarks and Folders to the clipboard
   */
  copy: function PC_copy() {
    var result = this._view.getResult();
    var oldViewer = result.viewer;
    try {
      result.viewer = null;
      var nodes = this._view.getCopyableSelection();

      var xferable =  Cc["@mozilla.org/widget/transferable;1"].
                      createInstance(Ci.nsITransferable);
      var foundFolder = false, foundLink = false;
      var copiedFolders = [];
      var placeString = mozURLString = htmlString = unicodeString = "";
    
      for (var i = 0; i < nodes.length; ++i) {
        var node = nodes[i];
        if (this._shouldSkipNode(node, copiedFolders))
          continue;
        if (PlacesUtils.nodeIsFolder(node))
          copiedFolders.push(node);
        
        function generateChunk(type, overrideURI) {
          var suffix = i < (nodes.length - 1) ? NEWLINE : "";
          var uri = overrideURI;
        
          if (PlacesUtils.nodeIsLivemarkContainer(node))
            uri = PlacesUtils.livemarks.getFeedURI(node.itemId).spec

          mozURLString += (PlacesUtils.wrapNode(node, PlacesUtils.TYPE_X_MOZ_URL,
                                                 uri) + suffix);
          unicodeString += (PlacesUtils.wrapNode(node, PlacesUtils.TYPE_UNICODE,
                                                 uri) + suffix);
          htmlString += (PlacesUtils.wrapNode(node, PlacesUtils.TYPE_HTML,
                                                 uri) + suffix);
        
          var placeSuffix = i < (nodes.length - 1) ? "," : "";
          return PlacesUtils.wrapNode(node, type, overrideURI) + placeSuffix;
        }

        // all items wrapped as TYPE_X_MOZ_PLACE
        placeString += generateChunk(PlacesUtils.TYPE_X_MOZ_PLACE);
      }

      function addData(type, data) {
        xferable.addDataFlavor(type);
        xferable.setTransferData(type, PlacesUtils._wrapString(data), data.length * 2);
      }
      // This order is _important_! It controls how this and other applications 
      // select data to be inserted based on type.
      if (placeString)
        addData(PlacesUtils.TYPE_X_MOZ_PLACE, placeString);
      if (mozURLString)
        addData(PlacesUtils.TYPE_X_MOZ_URL, mozURLString);
      if (unicodeString)
        addData(PlacesUtils.TYPE_UNICODE, unicodeString);
      if (htmlString)
        addData(PlacesUtils.TYPE_HTML, htmlString);

      if (placeString || unicodeString || htmlString || mozURLString) {
        PlacesUtils.clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);
      }
    }
    finally {
      if (oldViewer)
        result.viewer = oldViewer;
    }
  },

  /**
   * Cut Bookmarks and Folders to the clipboard
   */
  cut: function PC_cut() {
    this.copy();
    this.remove("Cut Selection");
  },

  /**
   * Paste Bookmarks and Folders from the clipboard
   */
  paste: function PC_paste() {
    // Strategy:
    // 
    // There can be data of various types (folder, separator, link) on the 
    // clipboard. We need to get all of that data and build edit transactions
    // for them. This means asking the clipboard once for each type and 
    // aggregating the results. 

    /**
     * Constructs a transferable that can receive data of specific types.
     * @param   types
     *          The types of data the transferable can hold, in order of
     *          preference.
     * @returns The transferable.
     */
    function makeXferable(types) {
      var xferable = 
          Cc["@mozilla.org/widget/transferable;1"].
          createInstance(Ci.nsITransferable);
      for (var i = 0; i < types.length; ++i) 
        xferable.addDataFlavor(types[i]);
      return xferable;
    }

    var clipboard = PlacesUtils.clipboard;

    var ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    /**
     * Gets a list of transactions to perform the paste of specific types.
     * @param   types
     *          The types of data to form paste transactions for
     * @returns An array of transactions that perform the paste.
     */
    function getTransactions(types) {
      var xferable = makeXferable(types);
      clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);

      var data = { }, type = { };
      try {
        xferable.getAnyTransferData(type, data, { });
        data = data.value.QueryInterface(Ci.nsISupportsString).data;
        var items = PlacesUtils.unwrapNodes(data, type.value);
        var transactions = [];
        var index = ip.index;
        for (var i = 0; i < items.length; ++i) {
          // adjusted to make sure that items are given the correct index -
          // transactions insert differently if index == -1
          if (ip.index > -1)
            index = ip.index + i;
          transactions.push(PlacesUtils.makeTransaction(items[i], type.value, 
                                                        ip.itemId, index,
                                                        true));
        }
        return transactions;
      }
      catch (e) {
        // getAnyTransferData will throw if there is no data of the specified
        // type on the clipboard. 
        // unwrapNodes will throw if the data that is present is malformed in
        // some way. 
        // In either case, don't fail horribly, just return no data.
      }
      return [];
    }

    // Get transactions to paste any folders, separators or links that might
    // be on the clipboard, aggregate them and execute them. 
    var transactions = getTransactions([PlacesUtils.TYPE_X_MOZ_PLACE,
                                        PlacesUtils.TYPE_X_MOZ_URL, 
                                        PlacesUtils.TYPE_UNICODE]);
    var txn = PlacesUtils.ptm.aggregateTransactions("Paste", transactions);
    PlacesUtils.ptm.commitTransaction(txn);
  }
};

function PlacesMenuDNDObserver(aView, aPopup) {
  this._view = aView;
  this._popup = aPopup;
  this._popup.addEventListener("draggesture", this, false);
  this._popup.addEventListener("dragover", this, false);
  this._popup.addEventListener("dragdrop", this, false);
  this._popup.addEventListener("dragexit", this, false);
}

/**
 * XXXmano-please-rewrite-me: This code was ported over from menu.xul in bug 399729.
 * Unsurprisngly it's still mostly broken due to bug 337761, thus I didn't bother
 * trying to cleaning up this extremely buggy  over-folder detection code yet.
 */
PlacesMenuDNDObserver.prototype = {
  _view: null,
  _popup: null,

  // Sub-menus should be opened when the mouse drags over them, and closed
  // when the mouse drags off.  The overFolder object manages opening and closing
  // of folders when the mouse hovers.
  _overFolder: {node: null, openTimer: null, hoverTime: 350, closeTimer: null},

  // If this menu's parent auto-opened it because it was dragged over, but didn't
  // close it because the mouse dragged into it, the menu should close itself
  // onDragExit.  This timer is set in dragExit to close the menu.
  _closeMenuTimer: null,

  _setTimer: function TBV_DO_setTimer(time) {
    var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(this, time, timer.TYPE_ONE_SHOT);
    return timer;
  },

  // Function to process all timer notifications.
  notify: function TBV_DO_notify(timer) {
    // Timer to open a submenu that's being dragged over.
    if (timer == this._overFolder.openTimer) {
      this._overFolder.node.lastChild.setAttribute("autoopened", "true");
      this._overFolder.node.lastChild.showPopup(this._overFolder.node);
      this._overFolder.openTimer = null;
    }

    // Timer to close a submenu that's been dragged off of.
    if (timer == this._overFolder.closeTimer) {
      // Only close the submenu if the mouse isn't being dragged over any
      // of its child menus.
      var draggingOverChild =
        PlacesControllerDragHelper.draggingOverChildNode(this._overFolder.node);
      if (draggingOverChild)
        this._overFolder.node = null;
      this._clearOverFolder();

      // Close any parent folders which aren't being dragged over.
      // (This is necessary because of the above code that keeps a folder
      // open while its children are being dragged over.)
      if (!draggingOverChild)
        this._closeParentMenus();
    }

    // Timer to close this menu after the drag exit.
    if (timer == this._closeMenuTimer) {
      if (!PlacesControllerDragHelper.draggingOverChildNode(this._popup)) {
        this._popup.hidePopup();
        // Close any parent menus that aren't being dragged over;
        // otherwise they'll stay open because they couldn't close
        // while this menu was being dragged over.
        this._closeParentMenus();
      }
    }
  },

  // Helper function to close all parent menus of this menu,
  // as long as none of the parent's children are currently being
  // dragged over.
  _closeParentMenus: function TBV_DO_closeParentMenus() {
    var parent = this._popup.parentNode;
    while (parent) {
      if (parent.nodeName == "menupopup" && parent._resultNode) {
        if (PlacesControllerDragHelper.draggingOverChildNode(parent.parentNode))
          break;
        parent.hidePopup();
      }
      parent = parent.parentNode;
    }
  },

  // The mouse is no longer dragging over the stored menubutton.
  // Close the menubutton, clear out drag styles, and clear all
  // timers for opening/closing it.
  _clearOverFolder: function TBV_DO_clearOverFolder() {
    if (this._overFolder.node && this._overFolder.node.lastChild) {
      if (!this._overFolder.node.lastChild.hasAttribute("dragover"))
        this._overFolder.node.lastChild.hidePopup();
      this._overFolder.node = null;
    }
    if (this._overFolder.openTimer) {
      this._overFolder.openTimer.cancel();
      this._overFolder.openTimer = null;
    }
    if (this._overFolder.closeTimer) {
      this._overFolder.closeTimer.cancel();
      this._overFolder.closeTimer = null;
    }
  },

  // This function returns information about where to drop when
  // dragging over this menu--insertion point, child index to drop
  // before, and folder to drop into.
  _getDropPoint: function TBV_DO_getDropPoint(event) {
    // Can't drop if the menu isn't a folder
    var resultNode = this._popup._resultNode;
    if (!PlacesUtils.nodeIsFolder(resultNode))
      return null;

    var dropPoint = { ip: null, beforeIndex: null, folderNode: null };
    // Loop through all the nodes to see which one this should
    // get dropped in/above/below.
    var start = 0;
    var end = this._popup.childNodes.length;
    if (this._popup == this._view && this._view.localName == "menupopup") {
      // Ignore static content at the top and bottom of the menu.
      start = this._view._startMarker + 1;
      if (this._view._endMarker != -1)
        end = this._view._endMarker;
    }

    for (var i = start; i < end; i++) {
      var xulNode = this._popup.childNodes[i];
      var nodeY = xulNode.boxObject.y - this._popup.boxObject.y;
      var nodeHeight = xulNode.boxObject.height;
      if (xulNode.node &&
          PlacesUtils.nodeIsFolder(xulNode.node) &&
          !PlacesUtils.nodeIsReadOnly(xulNode.node)) {
        // This is a folder. If the mouse is in the top 25% of the
        // node, drop above the folder.  If it's in the middle
        // 50%, drop into the folder.  If it's past that, drop below.
        if (event.clientY < nodeY + (nodeHeight * 0.25)) {
          // Drop above this folder.
          dropPoint.ip = new InsertionPoint(resultNode.itemId, i - start,
                                            -1);
          dropPoint.beforeIndex = i;
          return dropPoint;
        }
        else if (event.clientY < nodeY + (nodeHeight * 0.75)) {
          // Drop inside this folder.
          dropPoint.ip = new InsertionPoint(xulNode.node.itemId, -1, 1);
          dropPoint.beforeIndex = i;
          dropPoint.folderNode = xulNode;
          return dropPoint;
        }
      } else {
        // This is a non-folder node. If the mouse is above the middle,
        // drop above the folder.  Otherwise, drop below.
        if (event.clientY < nodeY + (nodeHeight / 2)) {
          // Drop above this bookmark.
          dropPoint.ip = new InsertionPoint(resultNode.itemId, i - start, -1);
          dropPoint.beforeIndex = i;
          return dropPoint;
        }
      }
    }
    // Should drop below the last node.
    dropPoint.ip = new InsertionPoint(resultNode.itemId, -1, 1);
    dropPoint.beforeIndex = -1;
    return dropPoint;
  },

  // This function clears all of the dragover styles that were set when
  // a menuitem was dragged over.
  _clearStyles: function TBV_DO_clearStyles() {
    this._popup.removeAttribute("dragover");
    for (var i = 0; i < this._popup.childNodes.length; i++) {
      this._popup.childNodes[i].removeAttribute("dragover-top");
      this._popup.childNodes[i].removeAttribute("dragover-bottom");
      this._popup.childNodes[i].removeAttribute("dragover-into");
    }
  },

  onDragStart: function TBV_DO_onDragStart(event, xferData, dragAction) {
    this._view._selection = event.target.node;
    this._view._cachedInsertionPoint = undefined;
    if (event.ctrlKey)
      dragAction.action = Ci.nsIDragService.DRAGDROP_ACTION_COPY;
    xferData.data = this._view.controller.getTransferData(dragAction.action);
  },

  canDrop: function TBV_DO_canDrop(event, session) {
    return PlacesControllerDragHelper.canDrop(this._view, -1);
  },

  onDragOver: function TBV_DO_onDragOver(event, flavor, session) {
    PlacesControllerDragHelper.currentDropTarget = event.target;
    var dropPoint = this._getDropPoint(event);
    if (dropPoint == null)
      return;

    this._clearStyles();
    if (dropPoint.folderNode) {
      // Dragging over a folder; set the appropriate styles.
      if (this._overFolder.node != dropPoint.folderNode) {
        this._clearOverFolder();
        this._overFolder.node = dropPoint.folderNode;
        this._overFolder.openTimer = this._setTimer(this._overFolder.hoverTime);
      }
      dropPoint.folderNode.setAttribute("dragover-into", "true");
    }
    else {
      // Dragging over a menuitem, set dragover-top/bottom to show where
      // the item will be dropped and clear out any old folder info.
      if (dropPoint.beforeIndex == -1) {
        if (this._popup == this._view && this._view.localName == "menupopup" &&
            this._popup._endMarker != -1) {
          this._popup.childNodes[this._popup._endMarker]
                     .setAttribute("dragover-top", "true");
        }
        else
          this._popup.lastChild.setAttribute("dragover-bottom", "true");
      }
      else {
        this._popup.childNodes[dropPoint.beforeIndex]
            .setAttribute("dragover-top", "true");
      }

      // Clear out old folder information
      this._clearOverFolder();
    }
    this._popup.setAttribute("dragover", "true");
  },

  onDrop: function TBV_DO_onDrop(event, dropData, session) {
    var dropPoint = this._getDropPoint(event);
    if (!dropPoint)
      return;

    PlacesControllerDragHelper.onDrop(null, this._view, dropPoint.ip);
  },

  onDragExit: function TBV_DO_onDragExit(event, session) {
    PlacesControllerDragHelper.currentDropTarget = null;
    this._clearStyles();
    // Close any folder being hovered over
    if (this._overFolder.node)
      this._overFolder.closeTimer = this._setTimer(this._overFolder.hoverTime);
    // The autoopened attribute is set when this folder was automatically
    // opened after the user dragged over it.  If this attribute is set,
    // auto-close the folder on drag exit.
    if (this._popup.hasAttribute("autoopened"))
      this._closeMenuTimer = this._setTimer(this._overFolder.hoverTime);
  },

  getSupportedFlavours: function TBV_DO_getSupportedFlavours() {
    var flavorSet = new FlavourSet();
    for (var i = 0; i < this._view.peerDropTypes.length; ++i)
      flavorSet.appendFlavour(this._view.peerDropTypes[i]);
    return flavorSet;
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
    case "draggesture":
      if (aEvent.target.localName != "menu" && aEvent.target.node) {
        // TODO--allow menu drag if shift (or alt??) key is down
        nsDragAndDrop.startDrag(aEvent, this);
      }
      break;
    case "dragover":
      nsDragAndDrop.dragOver(aEvent, this);
      break;
    case "dragdrop":
      nsDragAndDrop.drop(aEvent, this);
      break;
    case "dragexit":
      nsDragAndDrop.dragExit(aEvent, this);
      break;
    }
  }
}

/**
 * Handles drag and drop operations for views. Note that this is view agnostic!
 * You should not use PlacesController._view within these methods, since
 * the view that the item(s) have been dropped on was not necessarily active. 
 * Drop functions are passed the view that is being dropped on. 
 */
var PlacesControllerDragHelper = {

  /**
   * Determines if the mouse is currently being dragged over a child node of
   * this menu. This is necessary so that the menu doesn't close while the
   * mouse is dragging over one of its submenus
   * @param   node
   *          The container node
   * @returns true if the user is dragging over a node within the hierarchy of
   *          the container, false otherwise.
   */
  draggingOverChildNode: function PCDH_draggingOverChildNode(node) {
    var currentNode = this.currentDropTarget;
    while (currentNode) {
      if (currentNode == node)
        return true;
      currentNode = currentNode.parentNode;
    }
    return false;
  },

  /**
   * DOM Element currently being dragged over
   */
  currentDropTarget: null,

  /**
   * @returns The current active drag session. Returns null if there is none.
   */
  getSession: function VO__getSession() {
    var dragService = Cc["@mozilla.org/widget/dragservice;1"].
                      getService(Ci.nsIDragService);
    return dragService.getCurrentSession();
  },

  /**
   * Determines whether or not the data currently being dragged can be dropped
   * on the specified view. 
   * @param   view
   *          An object implementing the AVI
   * @param   orientation
   *          The orientation of the drop
   * @returns true if the data being dragged is of a type supported by the view
   *          it is being dragged over, false otherwise. 
   */
  canDrop: function PCDH_canDrop(view, orientation) {
    var root = view.getResult().root;
    if (PlacesUtils.nodeIsReadOnly(root) || 
        !PlacesUtils.nodeIsFolder(root))
      return false;

    var session = this.getSession();
    if (session) {
      if (orientation != NHRVO.DROP_ON)
        var types = view.peerDropTypes;
      else
        types = view.childDropTypes;
      for (var i = 0; i < types.length; ++i) {
        if (session.isDataFlavorSupported(types[i]))
          return true;
      }
    }
    return false;
  },

  /** 
   * Creates a Transferable object that can be filled with data of types
   * supported by a view. 
   * @param   session
   *          The active drag session
   * @param   view
   *          An object implementing the AVI that supplies a list of 
   *          supported droppable content types
   * @param   orientation
   *          The orientation of the drop
   * @returns An object implementing nsITransferable that can receive data
   *          dropped onto a view. 
   */
  _initTransferable: function PCDH__initTransferable(session, view, orientation) {
    var xferable = Cc["@mozilla.org/widget/transferable;1"].
                   createInstance(Ci.nsITransferable);
    if (orientation != NHRVO.DROP_ON) 
      var types = view.peerDropTypes;
    else
      types = view.childDropTypes;    
    for (var i = 0; i < types.length; ++i) {
      if (session.isDataFlavorSupported(types[i]))
        xferable.addDataFlavor(types[i]);
    }
    return xferable;
  },

  /**
   * Handles the drop of one or more items onto a view.
   * @param   sourceView
   *          The AVI-implementing object that started the drop. 
   * @param   targetView
   *          The AVI-implementing object that received the drop. 
   * @param   insertionPoint
   *          The insertion point where the items should be dropped
   */
  onDrop: function PCDH_onDrop(sourceView, targetView, insertionPoint) {
    var session = this.getSession();
    var copy = session.dragAction & Ci.nsIDragService.DRAGDROP_ACTION_COPY;
    var transactions = [];
    var xferable = this._initTransferable(session, targetView, 
                                          insertionPoint.orientation);
    var dropCount = session.numDropItems;

    var movedCount = 0;

    for (var i = 0; i < dropCount; ++i) {
      session.getData(xferable, i);

      var data = { }, flavor = { };
      xferable.getAnyTransferData(flavor, data, { });
      data.value.QueryInterface(Ci.nsISupportsString);

      // There's only ever one in the D&D case. 
      var unwrapped = PlacesUtils.unwrapNodes(data.value.data, 
                                              flavor.value)[0];
      var index = insertionPoint.index;

      // Adjust insertion index to prevent reversal of dragged items. When you
      // drag multiple elts upward: need to increment index or each successive
      // elt will be inserted at the same index, each above the previous.
      if ((index != -1) && ((index < unwrapped.index) ||
                           (unwrapped.folder && (index < unwrapped.folder.index)))) {
        index = index + movedCount;
        movedCount++;
      }

      transactions.push(PlacesUtils.makeTransaction(unwrapped,
                        flavor.value, insertionPoint.itemId,
                        index, copy));
    }

    var txn = PlacesUtils.ptm.aggregateTransactions("DropItems", transactions);
    PlacesUtils.ptm.commitTransaction(txn);
  }
};

function goUpdatePlacesCommands() {
  var placesController;
  try {
    // Or any other command...
    placesController = top.document.commandDispatcher
                          .getControllerForCommand("placesCmd_open");
  }
  catch(ex) { return; }

  function updatePlacesCommand(aCommand) {
    var enabled = false;
    if (placesController)
      enabled = placesController.isCommandEnabled(aCommand);
    goSetCommandEnabled(aCommand, enabled);
  }

  updatePlacesCommand("placesCmd_open");
  updatePlacesCommand("placesCmd_open:window");
  updatePlacesCommand("placesCmd_open:tab");
  updatePlacesCommand("placesCmd_new:folder");
  updatePlacesCommand("placesCmd_new:bookmark");
  updatePlacesCommand("placesCmd_new:livemark");
  updatePlacesCommand("placesCmd_new:separator");
  updatePlacesCommand("placesCmd_show:info");
  updatePlacesCommand("placesCmd_moveBookmarks");
  updatePlacesCommand("placesCmd_reload");
  updatePlacesCommand("placesCmd_reloadMicrosummary");
  updatePlacesCommand("placesCmd_sortBy:name");
}
