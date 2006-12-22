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

// These need to be kept in sync with the meaning of these roots in 
// default_places.html!
const ORGANIZER_ROOT_HISTORY_UNSORTED = "place:&beginTime=-2592000000000&beginTimeRef=1&endTime=7200000000&endTimeRef=2&type=1"
const ORGANIZER_ROOT_HISTORY = ORGANIZER_ROOT_HISTORY_UNSORTED + "&sort=" + Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
const ORGANIZER_ROOT_BOOKMARKS = "place:&folder=2&group=3&excludeItems=1";
const ORGANIZER_SUBSCRIPTIONS_QUERY = "place:&annotation=livemark%2FfeedURI";

// Place entries that are containers, e.g. bookmark folders or queries. 
const TYPE_X_MOZ_PLACE_CONTAINER = "text/x-moz-place-container";
// Place entries that are bookmark separators.
const TYPE_X_MOZ_PLACE_SEPARATOR = "text/x-moz-place-separator";
// Place entries that are not containers or separators
const TYPE_X_MOZ_PLACE = "text/x-moz-place";
// Place entries in shortcut url format (url\ntitle)
const TYPE_X_MOZ_URL = "text/x-moz-url";
// Place entries formatted as HTML anchors
const TYPE_HTML = "text/html";
// Place entries as raw URL text
const TYPE_UNICODE = "text/unicode";

// No change to the view, preserve current selection
const RELOAD_ACTION_NOTHING = 0;
// Inserting items new to the view, select the inserted rows
const RELOAD_ACTION_INSERT = 1;
// Removing items from the view, select the first item after the last selected
const RELOAD_ACTION_REMOVE = 2;
// Moving items within a view, don't treat the dropped items as additional 
// rows.
const RELOAD_ACTION_MOVE = 3;

#ifdef XP_MACOSX
// On Mac OSX, the transferable system converts "\r\n" to "\n\n", where we
// really just want "\n".
const NEWLINE= "\n";
#else
// On other platforms, the transferable system converts "\r\n" to "\n".
const NEWLINE = "\r\n";
#endif

function STACK(args) {
  var temp = arguments.callee.caller;
  while (temp) {
    LOG("NAME: " + temp.name + "\n");
    temp = temp.arguments.callee.caller;    
  }
}

/**
 * Represents an insertion point within a container where we can insert
 * items. 
 * @param   folderId
 *          The folderId of the parent container
 * @param   index
 *          The index within the container where we should insert
 * @param   orientation
 *          The orientation of the insertion. NOTE: the adjustments to the
 *          insertion point to accommodate the orientation should be done by
 *          the person who constructs the IP, not the user. The orientation
 *          is provided for informational purposes only!
 * @constructor
 */
function InsertionPoint(folderId, index, orientation) {
  this.folderId = folderId;
  this.index = index;
  this.orientation = orientation;
}
InsertionPoint.prototype.toString = function IP_toString() {
  return "[object InsertionPoint(folder:" + this.folderId + ",index:" + this.index + ",orientation:" + this.orientation + ")]";
};

/** 
 * A View Configuration
 */
function ViewConfig(peerDropTypes, childDropTypes, excludeItems, excludeQueries, 
                    expandQueries, peerDropIndex) {
  this.peerDropTypes = peerDropTypes;
  this.childDropTypes = childDropTypes;
  this.excludeItems = excludeItems;
  this.excludeQueries = excludeQueries;
  this.expandQueries = expandQueries;
  this.peerDropIndex = peerDropIndex;
}
ViewConfig.GENERIC_DROP_TYPES = 
  [TYPE_X_MOZ_PLACE_CONTAINER,
   TYPE_X_MOZ_PLACE_SEPARATOR, 
   TYPE_X_MOZ_PLACE,
   TYPE_X_MOZ_URL];

/**
 * Configures Views, applying some meta-model rules. These rules are model-like,
 * e.g. must apply everwhere a model is instantiated, but are not actually stored
 *      in the data model itself. For example, you can't drag leaf items onto the
 *      places root. This needs to be enforced automatically everywhere a view of
 *      that model is instantiated. 
 */
var ViewConfigurator = {
  rules: { 
    "folder=1": new ViewConfig([TYPE_X_MOZ_PLACE_CONTAINER], 
                               ViewConfig.GENERIC_DROP_TYPES,
                               true, false, false, 4)
  },
  
  /**
   * Applies rules to a specific view. 
   */
  configure: function PC_configure(view) {
    // Determine what place the view is showing.
    var place = view.place;
    
    // Find a ruleset that matches the current place.
    var rules = null;
    for (var test in this.rules) {
      if (place.indexOf(test) != -1) {
        rules = this.rules[test];
        break;
      }
    }
    
    // If rules are found, apply them. 
    if (rules) {
      view.peerDropTypes = rules.peerDropTypes;
      view.childDropTypes = rules.childDropTypes;
      view.excludeItems = rules.excludeItems;
      view.excludeQueries = rules.excludeQueries;
      view.expandQueries = rules.expandQueries;
      view.peerDropIndex = rules.peerDropIndex;
    }
  }
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
  
  isCommandEnabled: function PC_isCommandEnabled(command) {
    switch (command) {
    case "cmd_undo":
      return this._view.enableEditCommands && PlacesUtils.tm.numberOfUndoItems > 0;
    case "cmd_redo":
      return this._view.enableEditCommands && PlacesUtils.tm.numberOfRedoItems > 0;
    case "cmd_cut":
    case "cmd_delete":
      return this._view.enableEditCommands &&
             !this._selectionOverlapsSystemArea() &&
             this._hasRemovableSelection();
    case "cmd_copy":
      return this._view.enableEditCommands &&
             !this._selectionOverlapsSystemArea() &&
             this._view.hasSelection;
    case "cmd_paste":
      return this._view.enableEditCommands &&
             !this._selectionOverlapsSystemArea() &&
             this._canInsert() && 
             this._hasClipboardData() && this._canPaste();
    case "cmd_selectAll":
      if (this._view.enableEditCommands) {
        if (this._view.selType != "single") {
          var result = this._view.getResult();
          if (result) {
            var container = asContainer(result.root);
            if (container.childCount > 0);
              return true;
          }
        }
      }
      return false;
    case "placesCmd_open":
    case "placesCmd_open:window":
    case "placesCmd_open:tab":
      return this._view.selectedURINode &&
             !this._selectionOverlapsSystemArea();
    case "placesCmd_open:tabs":
      // We can open multiple links in tabs if there is either: 
      //  a) a single folder selected
      //  b) many links or folders selected
      // XXXben - inSysArea should be removed, and generally be replaced by 
      //          something that counts the number of selected links or the
      //          number of links in the folder and enables the command only if
      //          the number is less than some 'safe' amount.
      var selectedNode = this._view.selectedNode;
      var hasSingleSelection = this._view.hasSingleSelection;
      var singleFolderSelected = hasSingleSelection && 
                                 PlacesUtils.nodeIsFolder(selectedNode);
      return !this._selectionOverlapsSystemArea() &&
             (singleFolderSelected || !hasSingleSelection);
#ifdef MOZ_PLACES_BOOKMARKS
    case "placesCmd_new:folder":
      // New Folder - don't check selectionOverlapsSystemArea since we should
      // be able to create folders in the left list even when elements at the
      // top are selected.
      return this._canInsert() &&
             this._view.peerDropTypes.indexOf(TYPE_X_MOZ_PLACE_CONTAINER) != -1;
    case "placesCmd_new:bookmark":
      return !this._selectionOverlapsSystemArea() &&
             this._canInsert() &&
             this._view.peerDropTypes.indexOf(TYPE_X_MOZ_URL) != -1;
    case "placesCmd_new:separator":
      return !this._selectionOverlapsSystemArea() &&
             this._canInsert() &&
             this._view.peerDropTypes.indexOf(TYPE_X_MOZ_PLACE_SEPARATOR) != -1;
    case "placesCmd_show:info":
      if (!this._selectionOverlapsSystemArea() &&
          this._view.hasSingleSelection) {
        var selectedNode = this._view.selectedNode;
        if (PlacesUtils.nodeIsBookmark(selectedNode) ||
            PlacesUtils.nodeIsFolder(selectedNode))
          return true;
      }
      return false;
#endif
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
      this.openLinkInCurrentWindow();
      break;
    case "placesCmd_open:window":
      this.openLinkInNewWindow();
      break;
    case "placesCmd_open:tab":
      this.openLinkInNewTab();
      break;
    case "placesCmd_open:tabs":
      this.openLinksInTabs();
      break;
#ifdef MOZ_PLACES_BOOKMARKS
    case "placesCmd_new:folder":
      this.newFolder();
      break;
    case "placesCmd_new:bookmark":
      // XXX: not yet implemented
      // this.newBookmark();
      break;
    case "placesCmd_new:separator":
      this.newSeparator();
      break;
    case "placesCmd_show:info":
      this.showBookmarkPropertiesForSelection();
      break;
#endif
    }
  },

  onEvent: function PC_onEvent(eventName) {
    //LOG("onEvent: " + eventName);
  },
  
  /**
   * Updates the enabled state of a command element. 
   * @param   commandId
   *          The id of the command element to update
   * @param   enabled
   *          Whether or not the command element should be enabled.
   */
  _setEnabled: function PC__setEnabled(commandId, enabled) {
    var command = document.getElementById(commandId);
    // Prevents excessive setAttributes
    var disabled = command.hasAttribute("disabled");
    if (enabled && disabled)
      command.removeAttribute("disabled");
    else if (!enabled && !disabled)
      command.setAttribute("disabled", "true");
  },
  
  /**
   * Determine whether or not the selection can be removed, either by the 
   * delete or cut operations based on whether or not any of its contents
   * are non-removable. We don't need to worry about recursion here since it
   * is a policy decision that a removable item not be placed inside a non-
   * removable item. 
   * @returns true if the selection contains no nodes that cannot be removed,
   *          false otherwise. 
   */
  _hasRemovableSelection: function PC__hasRemovableSelection() {
    var v = this._view;
    NS_ASSERT(v, "No active view - cannot paste!");
    if (!v)
      return false;
    var nodes = v.getSelectionNodes();
    var root = v.getResult().root;

    for (var i = 0; i < nodes.length; ++i) {
      var parent = nodes[i].parent || root;
      // We don't call nodeIsReadOnly here, because nodeIsReadOnly means that
      // a node has children that cannot be edited, reordered or removed. Here,
      // we don't care if a node's children can't be reordered or edited, just
      // that they're removable. All history results have removable children
      // (based on the principle that any URL in the history table should be
      // removable), but some special bookmark folders may have non-removable
      // children, e.g. live bookmark folder children. It doesn't make sense
      // to delete a child of a live bookmark folder, since when the folder
      // refreshes, the child will return. 
      if (PlacesUtils.nodeIsFolder(parent)) {
        var readOnly = PlacesUtils.bookmarks.getFolderReadonly(asFolder(parent).folderId);
        if (readOnly)
          return !readOnly;
      }
    }
    if (!v.hasSelection)
      return !PlacesUtils.nodeIsReadOnly(root);
    return true;
  },
  
  /**
   * Determines whether or not the clipboard contains data that the active
   * view can support in a paste operation. 
   * @returns true if the clipboard contains data compatible with the active
   *          view, false otherwise. 
   */
  _hasClipboardData: function PC__hasClipboardData() {
    var types = this._view.peerDropTypes;
    var flavors = 
        Cc["@mozilla.org/supports-array;1"].
        createInstance(Ci.nsISupportsArray);
    for (var i = 0; i < types.length; ++i) {
      var cstring = 
          Cc["@mozilla.org/supports-cstring;1"].
          createInstance(Ci.nsISupportsCString);
      cstring.data = types[i];
      flavors.AppendElement(cstring);
    }
  
    var clipboard = 
        Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    return clipboard.hasDataMatchingFlavors(flavors, 
                                            Ci.nsIClipboard.kGlobalClipboard);
  },
  
  /**
   * Determines whether or not nodes can be inserted relative to the selection.
   */
  _canInsert: function PC__canInsert() {
    var v = this._view;
    NS_ASSERT(v, "No active view - cannot insert!");
    if (!v)
      return false;
    var nodes = v.getSelectionNodes();
    var root = v.getResult().root;
    for (var i = 0; i < nodes.length; ++i) {
      var parent = nodes[i].parent || root;
      if (PlacesUtils.nodeIsReadOnly(parent))
        return false;
    }
    // Even if there's no selection, we need to check the root. Otherwise 
    // commands may be enabled for history views when nothing is selected. 
    return !PlacesUtils.nodeIsReadOnly(root);
  },
  
  /**
   * Determines whether or not the selection intersects the read only "system"
   * portion of the display. 
   * @returns true if the selection intersects, false otherwise. 
   */
  _selectionOverlapsSystemArea: function PC__selectionOverlapsSystemArea() {
    var v = this._view;
    if (!v.hasSelection)
      return false;
    var nodes = v.getSelectionNodes();
    var root = v.getResult().root;
    for (var i = 0; i < nodes.length; ++i) {
      // We also don't care about nodes that aren't at the root level.
      if (nodes[i].parent != root || 
          PlacesUtils.getIndexOfNode(nodes[i]) >= v.peerDropIndex)
        return false;
    }
    return true;
  },

#ifdef MOZ_PLACES_BOOKMARKS
  /**
   * Updates commands for persistent sorting
   * @param   inSysArea
   *          true if the selection intersects the read only "system" area.
   * @param   hasSingleSelection
   *          true if only one item is selected in the view
   * @param   selectedNode
   *          The selected nsINavHistoryResultNode
   * @param   canInsert
   *          true if the item is a writable container that can be inserted 
   *          into
   */
  _updateSortCommands: 
  function PC__updateSortCommands(inSysArea, hasSingleSelection, selectedNode, 
                                  canInsert) {
    // Some views, like menupopups, destroy their result as they hide, but they
    // are still the "last-active" view. Don't barf. 
    var result = this._view.getResult();
    var viewIsFolder = result ? PlacesUtils.nodeIsFolder(result.root) : false;

    // Depending on the selection, the persistent sort command sorts the 
    // contents of the current folder (when the selection is mixed or leaf 
    // items like individual bookmarks are selected) or the contents of the
    // selected folder (if a single folder is selected).     
    var sortingChildren = false;
    var name = result.root.title;
    var sortFolder = result.root;
    if (selectedNode && selectedNode.parent) {
      name = selectedNode.parent.title;
      sortFolder = selectedNode.parent;
    }
    if (hasSingleSelection && PlacesUtils.nodeIsFolder(selectedNode)) {
      name = selectedNode.title;
      sortFolder = selectedNode;
      sortingChildren = true;
    }
    
    // Count the children of the container. If there aren't at least two, we 
    // don't want to enable the command since there's nothing to be sorted.
    // We need to get the unfiltered contents of the container to make this
    // determination, which means a new query, since the existing query may
    // be filtered (e.g. left list). 
    var enoughChildrenToSort = false;
    if (PlacesUtils.nodeIsFolder(sortFolder)) {
      var folder = asFolder(sortFolder);
      var contents = this.getFolderContents(folder.folderId, false, false);
      enoughChildrenToSort = contents.childCount > 1;
    }
    var metadata = this._buildSelectionMetadata();
    this._setEnabled("placesCmd_sortby:name", 
      (sortingChildren || !inSysArea) && canInsert && viewIsFolder && 
      !("mixed" in metadata) && enoughChildrenToSort);
    
    var strings = document.getElementById("placeBundle");
    var command = document.getElementById("placesCmd_sortby:name");
    
    if (name) {
      command.setAttribute("label", 
        strings.getFormattedString("sortByName", [name]));
    }
    else
      command.setAttribute("label", strings.getString("sortByNameGeneric"));
  },
#endif

  /**
   * Determines if the active view can support inserting items of a certain type.
   */
  _viewSupportsInsertingType: function PC__viewSupportsInsertingType(type) {
    var types = this._view.peerDropTypes;
    for (var i = 0; i < types.length; ++i) {
      if (types[i] == type.value) 
        return true;
    }
    return true;
  },
  
  /**
   * Looks at the data on the clipboard to see if it is paste-able. 
   * Paste-able data is:
   *   - in a format that the view can receive
   *   - not a set of URIs that is entirely already present in the view, 
   *     since we can only have one instance of a URI per container. 
   * @returns true if the data is paste-able, false if the clipboard data
   *          cannot be pasted
   */
  _canPaste: function PC__canPaste() {
    var xferable = 
        Cc["@mozilla.org/widget/transferable;1"].
        createInstance(Ci.nsITransferable);
    xferable.addDataFlavor(TYPE_X_MOZ_PLACE_CONTAINER);
    xferable.addDataFlavor(TYPE_X_MOZ_PLACE_SEPARATOR);
    xferable.addDataFlavor(TYPE_X_MOZ_PLACE);
    xferable.addDataFlavor(TYPE_X_MOZ_URL);
    
    var clipboard = Cc["@mozilla.org/widget/clipboard;1"].
                    getService(Ci.nsIClipboard);
    clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);
    
    try {
      // getAnyTransferData can throw if no data is available. 
      var data = { }, type = { };
      xferable.getAnyTransferData(type, data, { });
      data = data.value.QueryInterface(Ci.nsISupportsString).data;
      if (!this._viewSupportsInsertingType(type.value))
        return false;
      
      // unwrapNodes will throw if the data blob is malformed. 
      var nodes = PlacesUtils.unwrapNodes(data, type.value);
      
      var ip = this._view.insertionPoint;
      if (!ip)
        return false;
      var contents = PlacesUtils.getFolderContents(ip.folderId);
      var cc = contents.childCount;

      /**
       * Determines whether or not a node is a first-level child of a folder. 
       * @param   node
       *          The node to check
       * @returns true if the node is a child of the container at the top level, false 
       *          otherwise
       */
      function nodeIsInList(node) {
        // Anything that isn't a URI is paste-able many times (folders, 
        // separators)
        if (!PlacesUtils.nodeIsURI(node))
          return false;
        for (var i = 0; i < cc; ++i) {
          if (contents.getChild(i).uri == node.uri.spec)
            return true;
        }
        return false;
      }
      
      // Since the bookmarks data model enforces only one instance of a URI per
      // folder, it is not possible to paste a selection into a folder where 
      // all of the URIs already exist. Thus we need to return false to disable
      // the command for this case. If only some of the URIs are present, we 
      // can still paste the non-present URIs, so it's ok to enable the command. 
      var nodesInList = 0;
      // Sadly, this is O(N^2). 
      for (var i = 0; i < nodes.length; ++i) {
        if (nodeIsInList(nodes[i]))
          ++nodesInList;
      }
      return (nodesInList != nodes.length);
    }
    catch (e) {
      // Unwrap nodes failed, possibly because a field that should have 
      // contained a URI did not actually contain something that is 
      // parse-able as a URI. 
      return false;
    }
    return false;
  },

#ifdef MOZ_PLACES_BOOKMARKS
  /**
   * Updates Livemark Commands: Reload
   * @param   hasSingleSelection
   *          true if only one item is selected in the view
   * @param   selectedNode
   *          The selected nsINavHistoryResultNode
   */
  _updateLivemarkCommands: 
  function PC__updateLivemarkCommands(hasSingleSelection, selectedNode) {
    var isLivemarkItem = false;
    var strings = document.getElementById("placeBundle");
    var command = document.getElementById("placesCmd_reload");
    if (hasSingleSelection) {
      if (selectedNode.uri.indexOf("livemark%2F") != -1) {
        isLivemarkItem = true;
        command.setAttribute("label", strings.getString("livemarkReloadAll"));
      }
      else {
        var name;
        if (PlacesUtils.nodeIsLivemarkContainer(selectedNode)) {
          isLivemarkItem = true;
          name = selectedNode.title;
        }
        else if (PlacesUtils.nodeIsURI(selectedNode)) {
          var uri = PlacesUtils._uri(selectedNode.uri);
          isLivemarkItem = PlacesUtils.annotations.hasAnnotation(uri, "livemark/bookmarkFeedURI");
          if (isLivemarkItem)
            name = selectedNode.parent.title;
        }

        if (isLivemarkItem)
          command.setAttribute("label", strings.getFormattedString("livemarkReloadOne", [name]));
      }
    }
    
    if (!isLivemarkItem)
      command.setAttribute("label", strings.getString("livemarkReload"));
      
    this._setEnabled("placesCmd_reload", isLivemarkItem);  
  },
#endif

  /** 
   * Gather information about the selection according to the following
   * rules:
   *    "link"              single selection is URI
   *    "links"             all selected items are links, and there are at least 2
   *    "folder"            selection is a folder
   *    "query"             selection is a query
   *    "remotecontainer"   selection is a remote container
   *    "separator"         selection is a separator line
   *    "host"              selection is a host
   *    "mutable"           selection can have items inserted or reordered
   *    "mixed"             selection contains more than one type
   *    "allLivemarks"      selection is a query containing every livemark
   *    "multiselect"       seleciton contains more than one item
   * In addition, a property is set corresponding to each of the selected 
   * items' annotation names.
   *    
   * @returns an object with each of the properties above set if the selection
   *          matches that rule. 
   * Note: This can be slow, so don't call it anywhere performance critical!
   */
  _buildSelectionMetadata: function PC__buildSelectionMetadata() {
    var metadata = { };
    var v = this._view;
    var hasSingleSelection = v.hasSingleSelection;
    if (v.selectedURINode && hasSingleSelection)
      metadata["link"] = true;
    if (hasSingleSelection) {
      var selectedNode = v.selectedNode;
      if (PlacesUtils.nodeIsFolder(selectedNode))
        metadata["folder"] = true;
      if (PlacesUtils.nodeIsQuery(selectedNode))
        metadata["query"] = true;
      if (PlacesUtils.nodeIsRemoteContainer(selectedNode))
        metadata["remotecontainer"] = true;
      if (PlacesUtils.nodeIsSeparator(selectedNode))
        metadata["separator"] = true;
      if (PlacesUtils.nodeIsHost(selectedNode))
        metadata["host"] = true;
    }
    
    // Mutability is whether or not a container can have selected items
    // inserted or reordered. It does _not_ dictate whether or not the container
    // can have items removed from it, since some containers that aren't 
    // reorderable can have items removed from them, e.g. a history list. 
    //
    // The mutability property starts out set to true, and is removed if 
    // any component of the selection is found to be part of a readonly 
    // container.
    metadata["mutable"] = true;

    /**
     * Determines whether or not a node is a readonly folder. 
     * @param   node
     *          The node to test.
     * @returns true if the node is a readonly folder.
     */
    function folderIsReadOnly(node) {
      return PlacesUtils.nodeIsFolder(node) && 
             PlacesUtils.bookmarks.getFolderReadonly(asFolder(node).folderId);
    }
    
    var foundNonURI = false;
    var nodes = v.getSelectionNodes();
    var root = v.getResult().root;
    if (v.hasSelection) 
      var lastParent = nodes[0].parent, lastType = nodes[0].type;
    else {
      // If there is no selection, mutability is determined by the readonly-ness
      // of the result root. See note above on mutability.
      if (folderIsReadOnly(root))
        delete metadata["mutable"];
    }
    // Walk the selection, gathering metadata about the selected items.       
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      if (!PlacesUtils.nodeIsURI(node))
        foundNonURI = true;
      
      // If there is a selection, mutability is determined by the readonly-ness
      // of the selected item, or the parent of the selection. See note above
      // on mutability.
      if (PlacesUtils.nodeIsReadOnly(node) || folderIsReadOnly(node.parent || root))
        delete metadata["mutable"];
      
      var uri = null;
      if (PlacesUtils.nodeIsURI(node))
        uri = PlacesUtils._uri(node.uri);
      if (PlacesUtils.nodeIsFolder(node))
        uri = PlacesUtils.bookmarks.getFolderURI(asFolder(node).folderId);
      if (uri) {
        var names = PlacesUtils.annotations.getPageAnnotationNames(uri, { });
        for (var j = 0; j < names.length; ++j)
          metadata[names[i]] = true;
      }
      else if (PlacesUtils.nodeIsQuery(node)) {
        // Various queries might live in the left-hand side of the organizer window.
        // If this one happens to have collected all the livemark feeds, allow its
        // context menu to contain "Reload All Livemarks". That will usually only
        // mean the Subscriptions folder, but if some other folder happens to use
        // the same query, it's fine too.  Queries have very limited data (no 
        // annotations), so we're left checking the query URI directly.
        uri = PlacesUtils._uri(node.uri);
        if (uri.spec == ORGANIZER_SUBSCRIPTIONS_QUERY)
          metadata["allLivemarks"] = true;
      }
            
      if (nodes[i].parent != lastParent || nodes[i].type != lastType)
        metadata["mixed"] = true;
    }
    
    if (v.selType != "single")
      metadata["multiselect"] = true;
    if (!foundNonURI && nodes.length > 1)
      metadata["links"] = true;

    return metadata;
  },
  
  /** 
   * Determines if a menuitem should be shown or not by comparing the rules
   * that govern the item's display with the state of the selection. 
   * @param   metadata
   *          metadata about the selection. 
   * @param   rules
   *          rules that govern the item's display
   * @returns true if the conditions are satisfied and the item can be 
   *          displayed, false otherwise. 
   */
  _shouldShowMenuItem: function(metadata, rules) {
    for (var i = 0; i < rules.length; ++i) {
      if (rules[i] in metadata)
        return true;
    }
    return false;
  },

  /**
   * Build a context menu for the selection, ensuring that the content of the
   * selection is correct and enabling/disabling items according to the state
   * of the commands. 
   * @param   popup
   *          The menupopup to build children into.
   */
  buildContextMenu: function PC_buildContextMenu(popup) {
    // Determine availability/enabled state of commands
    var metadata = this._buildSelectionMetadata();
    
    var lastVisible = null;
    for (var i = 0; i < popup.childNodes.length; ++i) {
      var item = popup.childNodes[i];
      var rules = item.getAttribute("selection");
      item.hidden = !this._shouldShowMenuItem(metadata, rules.split("|"));
      if (!item.hidden)
        lastVisible = item;
    }
    if (lastVisible.localName == "menuseparator")
      lastVisible.hidden = true;
    
    return true;
  },
  
  /**
   * Select all links in the current view. 
   */
  selectAll: function() {
    this._view.selectAll();
  },

  /**
   * Loads the selected node's URL in the appropriate tab or window, given the user's
   * preference specified by modifier keys tracked by a DOM mouse event
   * @param   aEvent
   *          The DOM Mouse event with modifier keys set that track the user's
   *          preferred destination window or tab.
   */
  openSelectedNodeInBrowser: function PC_openSelectedNodeInBrowser(aEvent) {
    var node = this._view.selectedURINode;
    if (node) {
      var browser = this._getBrowserWindow();
      if (browser)
        browser.openUILink(node.uri, aEvent, false, false);
      else
        this._openBrowserWith(node.uri);
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
      PlacesUtils.showFolderProperties(asFolder(node).folderId);
    else if (node.uri)
      PlacesUtils.showBookmarkProperties(PlacesUtils._uri(node.uri));
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
   *
   * Reloads the livemarks associated with the selection.  For the "Subscriptions"
   * folder, reloads all livemarks; for a livemark folder, reloads its children;
   * for a single livemark, reloads its siblings (the children of its parent).
   */
  reloadSelectedLivemarks: function PC_reloadSelectedLivemarks() {
    var selectedNode = this._view.selectedNode;
    if (this._view.hasSingleSelection) {
      if (selectedNode.uri.indexOf("livemark%2F") != -1) {
        PlacesUtils.livemarks.reloadAllLivemarks();
      }
      else {
        var folder = null;
        if (PlacesUtils.nodeIsLivemarkContainer(selectedNode)) {
          folder = asFolder(selectedNode);
        }
        else if (PlacesUtils.nodeIsURI(selectedNode)) {
          var uri = PlacesUtils._uri(selectedNode.uri);
          var isLivemarkItem = PlacesUtils.annotations.hasAnnotation(uri, "livemark/bookmarkFeedURI");
          if (isLivemarkItem)
            folder = asFolder(selectedNode.parent);
        }
        if (folder)
          PlacesUtils.livemarks.reloadLivemarkFolder(folder.folderId);
      }
    }
  },

  /**
   * Gets the current active browser window.
   */
  _getBrowserWindow: function PC__getBrowserWindow() {
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
    return wm.getMostRecentWindow("navigator:browser");
  },
  
  /**
   * Opens a new browser window, showing the specified url. 
   */
  _openBrowserWith: function PC__openBrowserWith(url) {
    openDialog("chrome://browser/content/browser.xul", "_blank", 
               "chrome,all,dialog=no", url, null, null);
  },

  /**
   * Loads the selected URL in a new tab. 
   */
  openLinkInNewTab: function PC_openLinkInNewTab() {
    var node = this._view.selectedURINode;
    if (node) {
      var browser = this._getBrowserWindow();
      if (browser) 
        browser.openNewTabWith(node.uri, null, null);
      else
        this._openBrowserWith(node.uri);
    }
  },

  /**
   * Loads the selected URL in a new window.
   */
  openLinkInNewWindow: function PC_openLinkInNewWindow() {
    var node = this._view.selectedURINode;
    if (node) {
      var browser = this._getBrowserWindow();
      if (browser) 
        browser.openNewWindowWith(node.uri, null, null, false);
      else
        this._openBrowserWith(node.uri);
    }
  },

  /**
   * Loads the selected URL in the current window, replacing the Places page.
   */
  openLinkInCurrentWindow: function PC_openLinkInCurrentWindow() {
    var node = this._view.selectedURINode;
    if (node) {
      var browser = this._getBrowserWindow();
      if (browser)
        browser.loadURI(node.uri, null, null, false);
      else
        this._openBrowserWith(node.uri);
    }
  },
  
  /**
   * Gives the user a chance to cancel loading lots of tabs at once
   */
  _confirmOpenTabs: function(numTabsToOpen) {
    var pref = 
        Components.classes["@mozilla.org/preferences-service;1"].
        getService(Components.interfaces.nsIPrefBranch);

    const kWarnOnOpenPref = "browser.tabs.warnOnOpen";
    var reallyOpen = true;
    if (pref.getBoolPref(kWarnOnOpenPref)) {
      if (numTabsToOpen >= pref.getIntPref("browser.tabs.maxOpenBeforeWarn")) {
        var promptService =
            Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
            getService(Components.interfaces.nsIPromptService);

        // default to true: if it were false, we wouldn't get this far
        var warnOnOpen = { value: true };

        var messageKey = "tabs.openWarningMultipleBranded";
        var openKey = "tabs.openButtonMultiple";
        var strings = document.getElementById("placeBundle");
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
       
        var buttonPressed = promptService.confirmEx(window,
          strings.getString("tabs.openWarningTitle"),
          strings.getFormattedString(messageKey, 
            [numTabsToOpen, brandShortName]),
          (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0)
          + (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
          strings.getString(openKey),
          null, null,
          strings.getFormattedString("tabs.openWarningPromptMeBranded",
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
   * XXXben this needs to handle the case when there are no open browser windows
   * XXXben this function is really long, should be split apart. The codepaths 
   *        seem different between load folder in tabs and load selection in
   *        tabs, too. 
   * See: https://bugzilla.mozilla.org/show_bug.cgi?id=331908
   */
  openLinksInTabs: function PC_openLinksInTabs() {
    var node = this._view.selectedNode;
    if (this._view.hasSingleSelection && PlacesUtils.nodeIsFolder(node)) {
      // Check prefs to see whether to open over existing tabs.
      var doReplace = getBoolPref("browser.tabs.loadFolderAndReplace");
      var loadInBackground = getBoolPref("browser.tabs.loadBookmarksInBackground");
      // Get the start index to open tabs at

      // XXX todo: no-browser-window-case
      var browserWindow = this._getBrowserWindow();
      var browser = browserWindow.getBrowser();
      var tabPanels = browser.browsers;
      var tabCount = tabPanels.length;
      var firstIndex;
      // If browser.tabs.loadFolderAndReplace pref is set, load over all the
      // tabs starting with the first one.
      if (doReplace)
        firstIndex = 0;
      // If the pref is not set, only load over the blank tabs at the end, if any.
      else {
        for (firstIndex = tabCount - 1; firstIndex >= 0; --firstIndex) {
          var br = browser.browsers[firstIndex];
          if (br.currentURI.spec != "about:blank" ||
              br.webProgress.isLoadingDocument)
            break;
        }
        ++firstIndex;
      }

      // Open each uri in the folder in a tab.
      var index = firstIndex;
      asFolder(node);
      var wasOpen = node.containerOpen;
      node.containerOpen = true;
      var cc = node.childCount;

      // we can't just use |cc| as that might include
      // folders, separators, deleted bookmarks, etc.
      var numTabsToOpen = 0;
      for (var i = 0; i < cc; ++i) {
        var childNode = node.getChild(i);
        if (PlacesUtils.nodeIsURI(childNode))
          ++numTabsToOpen;
      }

      // restore the original state (temporarily) so that if we prompt
      // the user, the will not see a change to the open state.
      node.containerOpen = wasOpen;
      if (!this._confirmOpenTabs(numTabsToOpen))
        return;
      // ensure the container is open, we'll restore it again
      // to the original state when we are done
      node.containerOpen = true;

      for (var i = 0; i < cc; ++i) {
        var childNode = node.getChild(i);
        if (PlacesUtils.nodeIsURI(childNode)) {
          // If there are tabs to load over, load the uri into the next tab.
          if (index < tabCount)
            tabPanels[index].loadURI(childNode.uri);
          // Otherwise, create a new tab to load the uri into.
          else
            browser.addTab(childNode.uri);
          ++index;
        }
      }
      node.containerOpen = wasOpen;
      
      // If no bookmarks were loaded, just bail.
      if (index == firstIndex)
        return;

      // focus the first tab if prefs say to
      if (!loadInBackground || doReplace) {
        // Select the first tab in the group.
        // Set newly selected tab after quick timeout, otherwise hideous focus problems
        // can occur because new presshell is not ready to handle events
        function selectNewForegroundTab(browser, tab) {
          browser.selectedTab = tab;
        }
        var tabs = browser.mTabContainer.childNodes;
        setTimeout(selectNewForegroundTab, 0, browser, tabs[firstIndex]);
      }

      // Close any remaining open tabs that are left over.
      // (Always skipped when we append tabs)
      for (var i = tabCount - 1; i >= index; --i)
        browser.removeTab(tabs[i]);

      // and focus the content
      browserWindow.content.focus();
    }
    else {
      var nodes = this._view.getSelectionNodes();

      // we can't just use |nodes.length| as that might include
      // folders, separators, deleted bookmarks, etc.
      var numTabsToOpen = 0;
      for (var i = 0; i < nodes.length; ++i) {
        if (PlacesUtils.nodeIsURI(nodes[i]))
          ++numTabsToOpen;
      }

      if (!this._confirmOpenTabs(numTabsToOpen))
        return;

      for (var i = 0; i < nodes.length; ++i) {
        if (PlacesUtils.nodeIsURI(nodes[i]))
          this._getBrowserWindow().openNewTabWith(nodes[i].uri, null, null);
      }
    }
  },
  
  /**
   * Create a new Bookmark folder somewhere. Prompts the user for the name
   * of the folder. 
   */
  newFolder: function PC_newFolder() {
    var view = this._view;
    
    view.saveSelection(view.SAVE_SELECTION_INSERT);
    var ps =
        Cc["@mozilla.org/embedcomp/prompt-service;1"].
        getService(Ci.nsIPromptService);
    var bundle = document.getElementById("placeBundle");
    var title = bundle.getString("newFolderTitle");
    var text = bundle.getString("newFolderMessage");
    var value = { value: bundle.getString("newFolderDefault") };
    if (ps.prompt(window, title, text, value, null, { })) {
      var ip = view.insertionPoint;
      if (!ip)
        throw Cr.NS_ERROR_NOT_AVAILABLE;
      var txn = new PlacesCreateFolderTransaction(value.value, ip.folderId, 
                                                  ip.index);
      PlacesUtils.tm.doTransaction(txn);
      this._view.focus();
      view.restoreSelection();
    }
  },

  /**
   * Create a new Bookmark separator somewhere.
   */
  newSeparator: function PC_newSeparator() {
    var ip = this._view.insertionPoint;
    if (!ip)
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    var txn = new PlacesCreateSeparatorTransaction(ip.folderId, ip.index);
    PlacesUtils.tm.doTransaction(txn);
    this._view.focus();
  },

  /**
   * Creates a set of transactions for the removal of a range of items. A range is 
   * an array of adjacent nodes in a view.
   * @param   range
   *          An array of nodes to remove. Should all be adjacent. 
   * @param   transactions
   *          An array of transactions.
   */
  _removeRange: function PC__removeRange(range, transactions) {
    NS_ASSERT(transactions instanceof Array, "Must pass a transactions array");
    var index = PlacesUtils.getIndexOfNode(range[0]);
    
    var removedFolders = [];
    
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
    
    /**
     * Walk the list of folders we're removing in this delete operation, and
     * see if the selected node specified is already implicitly being removed 
     * because it is a child of that folder. 
     * @param   node
     *          Node to check for containment. 
     * @returns true if the node should be skipped, false otherwise. 
     */
    function shouldSkipNode(node) {
      for (var j = 0; j < removedFolders.length; ++j) {
        if (isContainedBy(node, removedFolders[j]))
          return true;
      }
      return false;          
    }
    
    for (var i = 0; i < range.length; ++i) {
      var node = range[i];
      if (shouldSkipNode(node))
        continue;
      
      if (PlacesUtils.nodeIsFolder(node)) {
        // TODO -- node.parent might be a query and not a folder.  See bug 324948
        var folder = asFolder(node);
        removedFolders.push(folder);
        transactions.push(new PlacesRemoveFolderTransaction(folder.folderId));
      }
      else if (PlacesUtils.nodeIsSeparator(node)) {
        // A Bookmark separator.
        transactions.push(new PlacesRemoveSeparatorTransaction(
          asFolder(node.parent).folderId, index));
      }
      else if (PlacesUtils.nodeIsFolder(node.parent)) {
        // A Bookmark in a Bookmark Folder.
        transactions.push(new PlacesRemoveItemTransaction(
          PlacesUtils._uri(node.uri), asFolder(node.parent).folderId, index));
      }
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
    for (var i = ranges.length - 1; i >= 0 ; --i)
      this._removeRange(ranges[i], transactions);
    if (transactions.length > 0) {
      var txn = new PlacesAggregateTransaction(txnName, transactions);
      PlacesUtils.tm.doTransaction(txn);
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
   * @param   txnName
   *          A name for the transaction if this is being performed
   *          as part of another operation.
   */
  remove: function PC_remove(txnName) {
    NS_ASSERT(txnName !== undefined, "Must supply Transaction Name");
    this._view.saveSelection(this._view.SAVE_SELECTION_REMOVE);

    // Delete the selected rows. Do this by walking the selection backward, so
    // that when undo is performed they are re-inserted in the correct order.
    var type = this._view.getResult().root.type; 
    if (PlacesUtils.nodeIsFolder(this._view.getResult().root))
      this._removeRowsFromBookmarks(txnName);
    else
      this._removeRowsFromHistory();

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
    var nodes = null;
    if (dragAction == Ci.nsIDragService.DRAGDROP_ACTION_COPY)
      nodes = this._view.getCopyableSelection();
    else
      nodes = this._view.getDragableSelection();
    var dataSet = new TransferDataSet();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];

      var data = new TransferData();
      function addData(type) {
        data.addDataForFlavour(type, PlacesUtils._wrapString(PlacesUtils.wrapNode(node, type)));
      }

      if (PlacesUtils.nodeIsFolder(node) || PlacesUtils.nodeIsQuery(node)) {
        // Look up this node's place: URI in the annotation service to see if 
        // it is a special, non-movable folder. 
        // XXXben: TODO

        addData(TYPE_X_MOZ_PLACE_CONTAINER);
      }
      else if (PlacesUtils.nodeIsSeparator(node)) {
        addData(TYPE_X_MOZ_PLACE_SEPARATOR);
      }
      else {
        // This order is _important_! It controls how this and other 
        // applications select data to be inserted based on type. 
        addData(TYPE_X_MOZ_PLACE);
        addData(TYPE_UNICODE);
        addData(TYPE_HTML);
        addData(TYPE_X_MOZ_URL);
      }
      dataSet.push(data);
    }
    return dataSet;
  },

  /**
   * Copy Bookmarks and Folders to the clipboard
   */
  copy: function() {
    var nodes = this._view.getCopyableSelection();

    var xferable = 
        Cc["@mozilla.org/widget/transferable;1"].
        createInstance(Ci.nsITransferable);
    var foundFolder = false, foundLink = false;
    var pcString = psString = placeString = mozURLString = htmlString = unicodeString = "";
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      function generateChunk(type) {
        var suffix = i < (nodes.length - 1) ? NEWLINE : "";
        return PlacesUtils.wrapNode(node, type) + suffix;
      }
      if (PlacesUtils.nodeIsFolder(node) || PlacesUtils.nodeIsQuery(node)) {
        pcString += generateChunk(TYPE_X_MOZ_PLACE_CONTAINER);
      }
      else if (PlacesUtils.nodeIsSeparator(node)) {
        psString += generateChunk(TYPE_X_MOZ_PLACE_SEPARATOR);
      }
      else {
        placeString += generateChunk(TYPE_X_MOZ_PLACE);
        mozURLString += generateChunk(TYPE_X_MOZ_URL);
        htmlString += generateChunk(TYPE_HTML);
        unicodeString += generateChunk(TYPE_UNICODE);
      }
    }

    function addData(type, data) {
      xferable.addDataFlavor(type);
      xferable.setTransferData(type, PlacesUtils._wrapString(data), data.length * 2);
    }
    // This order is _important_! It controls how this and other applications 
    // select data to be inserted based on type. 
    if (pcString)
      addData(TYPE_X_MOZ_PLACE_CONTAINER, pcString);
    if (psString)
      addData(TYPE_X_MOZ_PLACE_SEPARATOR, psString);
    if (placeString)
      addData(TYPE_X_MOZ_PLACE, placeString);
    if (mozURLString)
      addData(TYPE_X_MOZ_URL, mozURLString);
    if (unicodeString)
      addData(TYPE_UNICODE, unicodeString);
    if (htmlString)
      addData(TYPE_HTML, htmlString);
    
    if (pcString || psString || placeString || unicodeString || htmlString || 
        mozURLString) {
      var clipboard = 
          Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
      clipboard.setData(xferable, null, Ci.nsIClipboard.kGlobalClipboard);
    }
  },

  /**
   * Cut Bookmarks and Folders to the clipboard
   */
  cut: function() {
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

    var clipboard = Cc["@mozilla.org/widget/clipboard;1"].
                    getService(Ci.nsIClipboard);

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
        for (var i = 0; i < items.length; ++i) {
          transactions.push(PlacesUtils.makeTransaction(items[i], type.value, 
                                                        ip.folderId, ip.index, true));
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
    var transactions = 
        [].concat(getTransactions([TYPE_X_MOZ_PLACE_CONTAINER]),
                  getTransactions([TYPE_X_MOZ_PLACE_SEPARATOR]),
                  getTransactions([TYPE_X_MOZ_PLACE, TYPE_X_MOZ_URL, 
                                  TYPE_UNICODE]));
    var txn = new PlacesAggregateTransaction("Paste", transactions);
    PlacesUtils.tm.doTransaction(txn);
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
    var parent = view.getResult().root;
    if (PlacesUtils.nodeIsReadOnly(parent) || 
        !PlacesUtils.nodeIsFolder(parent))
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
   * Creates a Transeferable object that can be filled with data of types
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
      if (session.isDataFlavorSupported(types[i]));
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
    for (var i = dropCount - 1; i >= 0; --i) {
      session.getData(xferable, i);
    
      var data = { }, flavor = { };
      xferable.getAnyTransferData(flavor, data, { });
      data.value.QueryInterface(Ci.nsISupportsString);
      
      // There's only ever one in the D&D case. 
      var unwrapped = PlacesUtils.unwrapNodes(data.value.data, 
                                              flavor.value)[0];
      transactions.push(PlacesUtils.makeTransaction(unwrapped, 
                        flavor.value, insertionPoint.folderId, 
                        insertionPoint.index, copy));
    }

    var txn = new PlacesAggregateTransaction("DropItems", transactions);
    PlacesUtils.tm.doTransaction(txn);
  }
};

/**
 * Method and utility stubs for Place Edit Transactions
 */
function PlacesBaseTransaction() {
}
PlacesBaseTransaction.prototype = {
  bookmarks: Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
             getService(Ci.nsINavBookmarksService),
  _livemarks: null,
  get livemarks() {
    if (!this._livemarks) {
      this._livemarks =
        Cc["@mozilla.org/browser/livemark-service;2"].
        getService(Ci.nsILivemarkService);
    }
    return this._livemarks;
  },

  LOG: LOG,
  redoTransaction: function PIT_redoTransaction() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  get isTransient() {
    return false;
  },

  merge: function PIT_merge(transaction) {
    return false;
  }
};

/**
 * Performs several Places Transactions in a single batch. 
 */
function PlacesAggregateTransaction(name, transactions) {
  this._transactions = transactions;
  this._name = name;
  this.container = -1;
  this.redoTransaction = this.doTransaction;
}
PlacesAggregateTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,
  
  doTransaction: function() {
    this.LOG("== " + this._name + " (Aggregate) ==============");
    this.bookmarks.beginUpdateBatch();
    for (var i = 0; i < this._transactions.length; ++i) {
      var txn = this._transactions[i];
      if (this.container > -1) 
        txn.container = this.container;
      txn.doTransaction();
    }
    this.bookmarks.endUpdateBatch();
    this.LOG("== " + this._name + " (Aggregate Ends) =========");
  },
  
  undoTransaction: function() {
    this.LOG("== UN" + this._name + " (UNAggregate) ============");
    this.bookmarks.beginUpdateBatch();
    for (var i = this._transactions.length - 1; i >= 0; --i) {
      var txn = this._transactions[i];
      if (this.container > -1) 
        txn.container = this.container;
      txn.undoTransaction();
    }
    this.bookmarks.endUpdateBatch();
    this.LOG("== UN" + this._name + " (UNAggregate Ends) =======");
  }
};


/**
 * Create a new Folder
 */
function PlacesCreateFolderTransaction(name, container, index) {
  NS_ASSERT(index >= -1, "invalid insertion index");
  this._name = name;
  this.container = container;
  this._index = index;
  this._id = null;
  this.childTransactions = [];
  this.redoTransaction = this.doTransaction;
}
PlacesCreateFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PCFT_doTransaction() {
    this.LOG("Create Folder: " + this._name + " in: " + this.container + "," + this._index);
    this._id = this.bookmarks.createFolder(this.container, this._name, this._index);
    for (var i = 0; i < this.childTransactions.length; ++i) {
      var txn = this.childTransactions[i];
      txn.container = this._id;
      txn.doTransaction();
    }
  },

  undoTransaction: function PCFT_undoTransaction() {
    this.LOG("UNCreate Folder: " + this._name + " from: " + this.container + "," + this._index);
    this.bookmarks.removeFolder(this._id);
    for (var i = 0; i < this.childTransactions.length; ++i) {
      var txn = this.childTransactions[i];
      txn.container = this._id;
      txn.undoTransaction();
    }
  }
};

/**
 * Create a new Item
 */
function PlacesCreateItemTransaction(uri, container, index) {
  NS_ASSERT(index >= -1, "invalid insertion index");
  this._uri = uri;
  this.container = container;
  this._index = index;
  this.redoTransaction = this.doTransaction;
}
PlacesCreateItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PCIT_doTransaction() {
    this.LOG("Create Item: " + this._uri.spec + " in: " + this.container + "," + this._index);
    this.bookmarks.insertItem(this.container, this._uri, this._index);
  },

  undoTransaction: function PCIT_undoTransaction() {
    this.LOG("UNCreate Item: " + this._uri.spec + " from: " + this.container + "," + this._index);
    this.bookmarks.removeItem(this.container, this._uri);
  }
};

/**
 * Create a new Separator
 */
function PlacesCreateSeparatorTransaction(container, index) {
  NS_ASSERT(index >= -1, "invalid insertion index");
  this.container = container;
  this._index = index;
  this._id = null;
}
PlacesCreateSeparatorTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PIST_doTransaction() {
    this.LOG("Create separator in: " + this.container + "," + this._index);
    this._id = this.bookmarks.insertSeparator(this.container, this._index);
  },

  undoTransaction: function PIST_undoTransaction() {
    this.LOG("UNCreate separator from: " + this.container + "," + this._index);
    this.bookmarks.removeChildAt(this.container, this._index);
  }
};

/**
 * Move a Folder
 */
function PlacesMoveFolderTransaction(id, oldContainer, oldIndex, newContainer, newIndex) {
  NS_ASSERT(!isNaN(id + oldContainer + oldIndex + newContainer + newIndex), "Parameter is NaN!");
  NS_ASSERT(newIndex >= -1, "invalid insertion index");
  this._id = id;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this._newContainer = newContainer;
  this._newIndex = newIndex;
  this.redoTransaction = this.doTransaction;
}
PlacesMoveFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PMFT_doTransaction() {
    this.LOG("Move Folder: " + this._id + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this.bookmarks.moveFolder(this._id, this._newContainer, this._newIndex);
  },

  undoTransaction: function PMFT_undoTransaction() {
    this.LOG("UNMove Folder: " + this._id + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this.bookmarks.moveFolder(this._id, this._oldContainer, this._oldIndex);
  }
};

/**
 * Move an Item
 */
function PlacesMoveItemTransaction(uri, oldContainer, oldIndex, newContainer, newIndex) {
  this._uri = uri;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this._newContainer = newContainer;
  this._newIndex = newIndex;
  this.redoTransaction = this.doTransaction;
}
PlacesMoveItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PMIT_doTransaction() {
    this.LOG("Move Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this.bookmarks.removeItem(this._oldContainer, this._uri);
    this.bookmarks.insertItem(this._newContainer, this._uri, this._newIndex);
  },

  undoTransaction: function PMIT_undoTransaction() {
    this.LOG("UNMove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this.bookmarks.removeItem(this._newContainer, this._uri);
    this.bookmarks.insertItem(this._oldContainer, this._uri, this._oldIndex);
  }
};

/**
 * Remove a Folder
 * This is a little complicated. When we remove a container we need to remove 
 * all of its children. We can't just repurpose our existing transactions for
 * this since they cache their parent container id. Since the folder structure
 * is being removed, this id is being destroyed and when it is re-created will
 * likely have a different id.
 */

function PlacesRemoveFolderTransaction(id) {
  this._removeTxn = this.bookmarks.getRemoveFolderTransaction(id);
  this._id = id;
  this._transactions = []; // A set of transactions to remove content.
  this.redoTransaction = this.doTransaction;
}
PlacesRemoveFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 
  getFolderContents:
  function(aFolderId, aExcludeItems, aExpandQueries) {
    return PlacesUtils.getFolderContents(aFolderId, aExcludeItems, aExpandQueries);
  },

  /**
   * Create a flat, ordered list of transactions for a depth-first recreation
   * of items within this folder.
   */
  _saveFolderContents: function PRFT__saveFolderContents() {
    this._transactions = [];
    var contents = this.getFolderContents(this._id, false, false);
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);  
    for (var i = 0; i < contents.childCount; ++i) {
      var child = contents.getChild(i);
      var txn;
      if (child.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER) {
        var folder = asFolder(child);
        txn = new PlacesRemoveFolderTransaction(folder.folderId);
      }
      else if (child.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR) {
        txn = new PlacesRemoveSeparatorTransaction(this._id, i);
      }
      else {
        txn = new PlacesRemoveItemTransaction(ios.newURI(child.uri, null, null),
                                              this._id, i);
      }
      this._transactions.push(txn);
    }
  },

  doTransaction: function PRFT_doTransaction() {
    var title = this.bookmarks.getFolderTitle(this._id);
    this.LOG("Remove Folder: " + title);

    this._saveFolderContents();

    // Remove children backwards to preserve parent-child relationships.
    for (var i = this._transactions.length - 1; i >= 0; --i)
      this._transactions[i].doTransaction();
    
    // Remove this folder itself. 
    this._removeTxn.doTransaction();
  },

  undoTransaction: function PRFT_undoTransaction() {
    this._removeTxn.undoTransaction();
    
    var title = this.bookmarks.getFolderTitle(this._id);
    this.LOG("UNRemove Folder: " + title);
    
    // Create children forwards to preserve parent-child relationships.
    for (var i = 0; i < this._transactions.length; ++i)
      this._transactions[i].undoTransaction();
  }
};

/**
 * Remove an Item
 */
function PlacesRemoveItemTransaction(uri, oldContainer, oldIndex) {
  this._uri = uri;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this.redoTransaction = this.doTransaction;
}
PlacesRemoveItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 
  
  doTransaction: function PRIT_doTransaction() {
    this.LOG("Remove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex);
    this.bookmarks.removeItem(this._oldContainer, this._uri);
  },
  
  undoTransaction: function PRIT_undoTransaction() {
    this.LOG("UNRemove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex);
    this.bookmarks.insertItem(this._oldContainer, this._uri, this._oldIndex);
  }
};

/**
 * Remove a separator
 */
function PlacesRemoveSeparatorTransaction(oldContainer, oldIndex) {
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
}
PlacesRemoveSeparatorTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 

  doTransaction: function PRST_doTransaction() {
    this.LOG("Remove Separator from: " + this._oldContainer + "," + this._oldIndex);
    this.bookmarks.removeChildAt(this._oldContainer, this._oldIndex);
  },
  
  undoTransaction: function PRST_undoTransaction() {
    this.LOG("UNRemove Separator from: " + this._oldContainer + "," + this._oldIndex);
    this.bookmarks.insertSeparator(this._oldContainer, this._oldIndex);
  }
};

/**
 * Edit a bookmark's title.
 */
function PlacesEditItemTitleTransaction(uri, newTitle) {
  this._uri = uri;
  this._newTitle = newTitle;
  this._oldTitle = "";
  this.redoTransaction = this.doTransaction;
}
PlacesEditItemTitleTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PEITT_doTransaction() {
    this._oldTitle = this.bookmarks.getItemTitle(this._uri);
    this.bookmarks.setItemTitle(this._uri, this._newTitle);
  },

  undoTransaction: function PEITT_undoTransaction() {
    this.bookmarks.setItemTitle(this._uri, this._oldTitle);
  }
};

/**
 * Edit a folder's title.
 */
function PlacesEditFolderTitleTransaction(id, newTitle) {
  this._id = id;
  this._newTitle = newTitle;
  this._oldTitle = "";
  this.redoTransaction = this.doTransaction;
}
PlacesEditFolderTitleTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PEFTT_doTransaction() {
    this._oldTitle = this.bookmarks.getFolderTitle(this._id);
    this.bookmarks.setFolderTitle(this._id, this._newTitle);
  },

  undoTransaction: function PEFTT_undoTransaction() {
    this.bookmarks.setFolderTitle(this._id, this._oldTitle);
  }
};

/**
 * Edit a bookmark's keyword.
 */
function PlacesEditBookmarkKeywordTransaction(uri, newKeyword) {
  this._uri = uri;
  this._newKeyword = newKeyword;
  this._oldKeyword = "";
  this.redoTransaction = this.doTransaction;
}
PlacesEditBookmarkKeywordTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PEBKT_doTransaction() {
    this._oldKeyword = this.bookmarks.getKeywordForURI(this._uri);
    this.bookmarks.setKeywordForURI(this._uri, this._newKeyword);
  },

  undoTransaction: function PEBKT_undoTransaction() {
    this.bookmarks.setKeywordForURI(this._uri, this._oldKeyword);
  }
};

/**
 * Edit a live bookmark's site URI.
 */
function PlacesEditLivemarkSiteURITransaction(folderId, uri) {
  this._folderId = folderId;
  this._newURI = uri;
  this._oldURI = null;
  this.redoTransaction = this.doTransaction;
}
PlacesEditLivemarkSiteURITransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PELSUT_doTransaction() {
    this._oldURI = this.livemarks.getSiteURI(this._folderId);
    this.livemarks.setSiteURI(this._folderId, this._newURI);
  },

  undoTransaction: function PELSUT_undoTransaction() {
    this.livemarks.setSiteURI(this._folderId, this._oldURI);
  }
};

/**
 * Edit a live bookmark's feed URI.
 */
function PlacesEditLivemarkFeedURITransaction(folderId, uri) {
  this._folderId = folderId;
  this._newURI = uri;
  this._oldURI = null;
  this.redoTransaction = this.doTransaction;
}
PlacesEditLivemarkFeedURITransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PELFUT_doTransaction() {
    this._oldURI = this.livemarks.getFeedURI(this._folderId);
    this.livemarks.setFeedURI(this._folderId, this._newURI);
    this.livemarks.reloadLivemarkFolder(this._folderId);
  },

  undoTransaction: function PELFUT_undoTransaction() {
    this.livemarks.setFeedURI(this._folderId, this._oldURI);
    this.livemarks.reloadLivemarkFolder(this._folderId);
  }
};

/**
 * Edit a bookmark's microsummary.
 */
function PlacesEditBookmarkMicrosummaryTransaction(uri, newMicrosummary) {
  this._uri = uri;
  this._newMicrosummary = newMicrosummary;
  this._oldMicrosummary = null;
  this.redoTransaction = this.doTransaction;
}
PlacesEditBookmarkMicrosummaryTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,
  
  mss: Cc["@mozilla.org/microsummary/service;1"].
       getService(Ci.nsIMicrosummaryService),

  doTransaction: function PEBMT_doTransaction() {
    this._oldMicrosummary = this.mss.getMicrosummary(this._uri);
    if (this._newMicrosummary)
      this.mss.setMicrosummary(this._uri, this._newMicrosummary);
    else
      this.mss.removeMicrosummary(this._uri);
  },

  undoTransaction: function PEBMT_undoTransaction() {
    if (this._oldMicrosummary)
      this.mss.setMicrosummary(this._uri, this._oldMicrosummary);
    else
      this.mss.removeMicrosummary(this._uri);
  }
};

function goUpdatePlacesCommands() {
  goUpdateCommand("placesCmd_open");
  goUpdateCommand("placesCmd_open:window");
  goUpdateCommand("placesCmd_open:tab");
  goUpdateCommand("placesCmd_open:tabs");
#ifdef MOZ_PLACES_BOOKMARKS
  goUpdateCommand("placesCmd_new:folder");
  goUpdateCommand("placesCmd_new:bookmark");
  goUpdateCommand("placesCmd_new:separator");
  goUpdateCommand("placesCmd_show:info");
  // XXXmano todo: sort and livemarks commands handling
#endif
}
