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
 * The Original Code is the Places Command Controller.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

var gTraceOnAssert = true;

// XXXben - develop this further into a multi-purpose assertion system. 
function ASSERT(condition, message) {
  if (!condition) {
    var caller = arguments.callee.caller;
    var str = "ASSERT: ";
    str += message;
    LOG(str);
    var assertionText = str + "\n";
    
    var stackText = "Stack Trace: \n";
    if (gTraceOnAssert) {
      var count = 0;
      while (caller) {
        stackText += count++ + ":" + caller.name + "(";
        for (var i = 0; i < caller.arguments.length; ++i) {
          var arg = caller.arguments[i];
          stackText += arg;
          if (i < caller.arguments.length - 1)
            stackText += ",";
        }
        stackText += ")\n";
        caller = caller.arguments.callee.caller;
      }
    }
    var ps = 
        Cc["@mozilla.org/embedcomp/prompt-service;1"].
        getService(Ci.nsIPromptService);
    ps.alert(window, "Assertion Failed", assertionText + stackText);
  }
}

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

const NHRVO = Ci.nsINavHistoryResultViewObserver;

const SELECTION_CONTAINS_URI = 0x01;
const SELECTION_CONTAINS_CONTAINER = 0x02;
const SELECTION_IS_OPEN_CONTAINER = 0x04;
const SELECTION_IS_CLOSED_CONTAINER = 0x08;
const SELECTION_IS_CHANGEABLE = 0x10;
const SELECTION_IS_REMOVABLE = 0x20;
const SELECTION_IS_MOVABLE = 0x40;

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

const NEWLINE = "\r\n";

function STACK(args) {
  var temp = arguments.callee.caller;
  while (temp) {
    LOG("NAME: " + temp.name);
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

/**
 * Initialization Configuration for a View
 * @constructor
 */
function ViewConfig(dropTypes, dropOnTypes, excludeItems, 
                    expandQueries, firstDropIndex, filterTransactions) {
  this.dropTypes = dropTypes;
  this.dropOnTypes = dropOnTypes;
  this.excludeItems = excludeItems;
  this.expandQueries = expandQueries;
  this.firstDropIndex = firstDropIndex;
  this.filterTransactions = filterTransactions;
}
ViewConfig.GENERIC_DROP_TYPES = [TYPE_X_MOZ_PLACE_CONTAINER,
                                 TYPE_X_MOZ_PLACE_SEPARATOR, TYPE_X_MOZ_PLACE,
                                 TYPE_X_MOZ_URL];

/**
 * Manages grouping options for a particular view type. 
 * @param   pref
 *          The preference that stores these grouping options. 
 * @param   defaultGroupings
 *          The default groupings to be used for views of this type. 
 * @param   serializable
 *          An object bearing a serialize and deserialize method that
 *          read and write the object's string representation from/to
 *          preferences.
 * @constructor
 */
function PrefHandler(pref, defaultValue, serializable) {
  this._pref = pref;
  this._defaultValue = defaultValue;
  this._serializable = serializable;

  this._pb = 
    Cc["@mozilla.org/preferences-service;1"].
    getService(Components.interfaces.nsIPrefBranch2);
  this._pb.addObserver(this._pref, this, false);
}
PrefHandler.prototype = {
  /**
   * Clean up when the window is going away to avoid leaks. 
   */
  destroy: function PC_PH_destroy() {
    this._pb.removeObserver(this._pref, this);
  },

  /** 
   * Observes changes to the preferences.
   * @param   subject
   * @param   topic
   *          The preference changed notification
   * @param   data
   *          The preference that changed
   */
  observe: function PC_PH_observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data == this._pref)
      this._value = null;
  },
  
  /**
   * The cached value, null if it needs to be rebuilt from preferences.
   */
  _value: null,

  /** 
   * Get the preference value, reading from preferences if necessary. 
   */
  get value() { 
    if (!this._value) {
      if (this._pb.prefHasUserValue(this._pref)) {
        var valueString = this._pb.getCharPref(this._pref);
        this._value = this._serializable.deserialize(valueString);
      }
      else
        this._value = this._defaultValue;
    }
    return this._value;
  },
  
  /**
   * Stores a value in preferences. 
   * @param   value
   *          The data to be stored. 
   */
  set value(value) {
    if (value != this._value)
      this._pb.setCharPref(this._pref, this._serializable.serialize(value));
    return value;
  },
};

function QI_node(node, iid) {
  var result = null;
  try {
    result = node.QueryInterface(iid);
  }
  catch (e) {
  }
  ASSERT(result, "Node QI Failed");
  return result;
}
function asFolder(node)   { return QI_node(node, Ci.nsINavHistoryFolderResultNode);   }
function asVisit(node)    { return QI_node(node, Ci.nsINavHistoryVisitResultNode);    }
function asFullVisit(node){ return QI_node(node, Ci.nsINavHistoryFullVisitResultNode);}
function asContainer(node){ return QI_node(node, Ci.nsINavHistoryContainerResultNode);}
function asQuery(node)    { return QI_node(node, Ci.nsINavHistoryQueryResultNode);    }

/**
 * The Master Places Controller
 */
var PlacesController = {
  /**
   * Makes a URI from a spec.
   * @param   spec
   *          The string spec of the URI
   * @returns A URI object for the spec. 
   */
  _uri: function PC__uri(spec) {
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    return ios.newURI(spec, null, null);
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
   * Generates a HistoryResultNode for the contents of a folder. 
   * @param   folderId
   *          The folder to open
   * @param   excludeItems
   *          True to hide all items (individual bookmarks). This is used on
   *          the left places pane so you just get a folder hierarchy.
   * @param   expandQueries
   *          True to make query items expand as new containers. For managing,
   *          you want this to be false, for menus and such, you want this to
   *          be true.
   * @returns A HistoryResultNode containing the contents of the folder. 
   */
  getFolderContents: function PC_getFolderContents(folderId, excludeItems, expandQueries) {
    var query = this._hist.getNewQuery();
    query.setFolders([folderId], 1);
    var options = this._hist.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    options.excludeItems = excludeItems;
    options.expandQueries = expandQueries;
    var result = this._hist.executeQuery(query, options);
    result.root.containerOpen = true;
    return asContainer(result.root);
  },
  
  /**
   * Gets a place: URI for the given queries and options. 
   * @param   queries
   *          An array of NavHistoryQueries
   * @param   options
   *          A NavHistoryQueryOptions object
   * @returns A place: URI encoding the parameters. 
   */
  getPlaceURI: function PC_getPlaceURI(queries, options) {
    var queryString = this._hist.queriesToQueryString(queries, queries.length,
                                                      options);
    return this._uri(queryString);
  },
  
  /**
   * The currently active Places view. 
   */
  _activeView: null,
  get activeView() {
    return this._activeView;
  },
  set activeView(activeView) {
    this._activeView = activeView;
    return this._activeView;
  },
  
  /**
   * The top window
   */
  topWindow: null,
  
  /**
   * The Transaction Manager for this window.
   */
  tm: null,
  
  /**
   * The current groupable Places view. This is tracked independently of the 
   * |activeView| because the activeView may change to something that isn't
   * groupable, but clicking the Grouping buttons should still work. 
   */
  _groupableView: null,
  get groupableView() {
    return this._groupableView;
  },
  set groupableView(groupableView) {
    this._groupableView = groupableView;
    return this._groupableView;
  },
  
  isCommandEnabled: function PC_isCommandEnabled(command) {
    //LOG("isCommandEnabled: " + command);
    if (command == "cmd_undo")
      return this.tm.numberOfUndoItems > 0;
    if (command == "cmd_redo")
      return this.tm.numberOfRedoItems > 0;
    return document.getElementById(command).getAttribute("disabled") == "true";
  },

  supportsCommand: function PC_supportsCommand(command) {
    //LOG("supportsCommand: " + command);
    if (command == "cmd_undo" || command == "cmd_redo")
      return true;
    return document.getElementById(command) != null;
  },

  doCommand: function PC_doCommand(command) {
    LOG("doCommand: " + command);
    if (command == "cmd_undo")
      this.tm.undoTransaction();
    else if (command == "cmd_redo")
      this.tm.redoTransaction();
  },

  onEvent: function PC_onEvent(eventName) {
    //LOG("onEvent: " + eventName);
  },
  
  /**
   * Updates the enabled state of a command element. 
   * @param   command
   *          The id of the command element to update
   * @param   enabled
   *          Whether or not the command element should be enabled.
   */
  _setEnabled: function PC__setEnabled(command, enabled) {
    var command = document.getElementById(command);
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
    ASSERT(this._activeView, "No active view - cannot paste!");
    if (!this._activeView)
      return false;
    var nodes = this._activeView.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      var parent = nodes[i].parent || this._activeView.getResult().root;
      if (this.nodeIsReadOnly(parent))
        return false;
    }
    return true;
  },
  
  /**
   * Determines whether or not the clipboard contains data that the active
   * view can support in a paste operation. 
   * @returns true if the clipboard contains data compatible with the active
   *          view, false otherwise. 
   */
  _hasClipboardData: function PC__hasClipboardData() {
    var types = this._activeView.supportedDropTypes;
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
    ASSERT(this._activeView, "No active view - cannot insert!");
    if (!this._activeView)
      return false;
    var nodes = this._activeView.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      var parent = nodes[i].parent || this._activeView.getResult().root;
      if (this.nodeIsReadOnly(parent))
        return false;
    }
    return true;
  },
  
  /**
   * Determines whether or not the paste command is enabled, based on the 
   * content of the clipboard and the selection within the active view.
   */
  _canPaste: function PC__canPaste() {
    if (this._canInsert()) 
      return this._hasClipboardData();
    return false;
  },
  
  /**
   * Determines whether or not a ResultNode is a Bookmark folder or not. 
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a Bookmark folder, false otherwise
   */
  nodeIsFolder: function PC_nodeIsFolder(node) {
    ASSERT(node, "null node");
    return (node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER);
  },
  
  /**
   * Determines whether or not a ResultNode is a Bookmark separator.
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a Bookmark separator, false otherwise
   */
  nodeIsSeparator: function PC_nodeIsSeparator(node) {
    ASSERT(node, "null node");
    return (node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR);
  },

  /**
   * Determines whether or not a ResultNode is a URL item or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a URL item, false otherwise
   */
  nodeIsURI: function PC_nodeIsURI(node) {
    ASSERT(node, "null node");
    const NHRN = Ci.nsINavHistoryResultNode;
    return node.type == NHRN.RESULT_TYPE_URI ||
           node.type == NHRN.RESULT_TYPE_VISIT ||
           node.type == NHRN.RESULT_TYPE_FULL_VISIT;
  },
  
  /**
   * Determines whether or not a ResultNode is a Query item or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a Query item, false otherwise
   */
  nodeIsQuery: function PC_nodeIsQuery(node) {
    ASSERT(node, "null node");
    return node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
  },
  
  /**
   * Determines if a node is read only (children cannot be inserted, sometimes
   * they cannot be removed depending on the circumstance)
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is readonly, false otherwise
   */
  nodeIsReadOnly: function PC_nodeIsReadOnly(node) {
    ASSERT(node, "null node");
    if (this.nodeIsFolder(node))
      return this._bms.getFolderReadonly(asFolder(node).folderId);
    else if (this.nodeIsQuery(node))
      return asQuery(node).childrenReadOnly;
    return false;
  },

  /**
   * Determines whether or not a ResultNode is a host folder or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a host item, false otherwise
   */
  nodeIsHost: function PC_nodeIsHost(node) {
    return node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_HOST;
  },

  /**
   * Determines whether or not a ResultNode is a container item or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a container item, false otherwise
   */
  nodeIsContainer: function PC_nodeIsContainer(node) {
    const NHRN = Ci.nsINavHistoryResultNode;
    return node.type == NHRN.RESULT_TYPE_HOST ||
           node.type == NHRN.RESULT_TYPE_QUERY ||
           node.type == NHRN.RESULT_TYPE_FOLDER ||
           node.type == NHRN.RESULT_TYPE_REMOTE_CONTAINER;
  },

  /**
   * Determines whether or not a ResultNode is a remotecontainer item or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a container item, false otherwise
   */
  nodeIsRemoteContainer: function PC_nodeIsRemoteContainer(node) {
    const NHRN = Ci.nsINavHistoryResultNode;
    if (node.type == NHRN.RESULT_TYPE_REMOTE_CONTAINER)
      return true;
    if (node.type == NHRN.RESULT_TYPE_FOLDER)
      return asFolder(node).folderType != "";
    
    return false;
  },
  
  /**
   * Updates commands on focus/selection change to reflect the enabled/
   * disabledness of commands in relation to the state of the selection. 
   */
  onCommandUpdate: function PC_onCommandUpdate() {
    if (!this._activeView) {
      // Initial update, no view is set yet
      return;
    }

    // Select All
    this._setEnabled("placesCmd_select:all", 
      this._activeView.getAttribute("seltype") != "single");
    // Show Info
    var hasSingleSelection = this._activeView.hasSingleSelection;
    this._setEnabled("placesCmd_show:info", hasSingleSelection);
    // Cut
    var removableSelection = this._hasRemovableSelection();
    this._setEnabled("placesCmd_edit:cut", removableSelection);
    this._setEnabled("placesCmd_edit:delete", removableSelection);
    // Copy
    this._setEnabled("placesCmd_edit:copy", this._activeView.hasSelection);
    // Paste
    this._setEnabled("placesCmd_edit:paste", this._canPaste());
    // Open
    var hasSelectedURI = this._activeView.selectedURINode != null;
    this._setEnabled("placesCmd_open", hasSelectedURI);
    this._setEnabled("placesCmd_open:window", hasSelectedURI);
    this._setEnabled("placesCmd_open:tab", hasSelectedURI);
    
    // We can open multiple links in tabs if there is either: 
    //  a) a single folder selected
    //  b) many links or folders selected
    var singleFolderSelected = hasSingleSelection && 
      this.nodeIsFolder(this._activeView.selectedNode);
    this._setEnabled("placesCmd_open:tabs", 
      singleFolderSelected || !hasSingleSelection);
    
    // Some views, like menupopups, destroy their result as they hide, but they
    // are still the "last-active" view. Don't barf. 
    var result = this._activeView.getResult();
    var viewIsFolder = result ? this.nodeIsFolder(result.root) : false;
    var canInsert = this._canInsert();

    // Persistent Sort
    this._setEnabled("placesCmd_sortby:name", viewIsFolder && canInsert);
    // New Folder
    this._setEnabled("placesCmd_new:folder", viewIsFolder && canInsert);
    // New Separator
    this._setEnabled("placesCmd_new:separator", viewIsFolder && canInsert);
    // Feed Reload
    this._setEnabled("placesCmd_reload", false);
  },
  
  /** 
   * Gather information about the selection according to the following
   * rules: 
   * Selection Grammar: 
   *    is-link       "link"
   *    is-links      "links"
   *    is-folder     "folder"
   *    is-mutable    "mutable"
   *    is-removable  "removable"
   *    is-multiselect"multiselect"
   *    is-container  "remotecontainer"
   * @returns an object with each of the properties above set if the selection
   *          matches that rule. 
   */
  _buildSelectionMetadata: function PC__buildSelectionMetadata() {
    var metadata = { mixed: true };
    
    var hasSingleSelection = this._activeView.hasSingleSelection;
    if (this._activeView.selectedURINode && hasSingleSelection)
      metadata["link"] = true;
    if (hasSingleSelection) {
      var selectedNode = this._activeView.selectedNode;
      if (this.nodeIsFolder(selectedNode))
        metadata["folder"] = true;
      if (this.nodeIsRemoteContainer(selectedNode))
        metadata["remotecontainer"] = true;
    }
    
    var foundNonLeaf = false;
    var nodes = this._activeView.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      if (node.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_URI)
        foundNonLeaf = true;
      if (!node.readonly && 
           node.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_REMOTE_CONTAINER)
        metadata["mutable"] = true;
    }
    if (this._activeView.getAttribute("seltype") != "single")
      metadata["multiselect"] = true;
    if (!foundNonLeaf && nodes.length > 1)
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
    if (document.popupNode.hasAttribute("view")) {
      var view = document.popupNode.getAttribute("view");
      this.activeView = document.getElementById(view);
    }
    
    // Determine availability/enabled state of commands
    var metadata = this._buildSelectionMetadata();
    var lastVisible = null;
    for (var i = 0; i < popup.childNodes.length; ++i) {
      var item = popup.childNodes[i];
      var rules = item.getAttribute("selection")
      item.hidden = !this._shouldShowMenuItem(metadata, rules.split("|"));
      if (!item.hidden)
        lastVisible = item;
      if (item.hasAttribute("command")) {
        var disabled = !this.isCommandEnabled(item.getAttribute("command"));
        item.setAttribute("disabled", disabled);
      }
    }
    if (lastVisible.localName == "menuseparator")
      lastVisible.hidden = true;
    
    return true;
  },
  
  /**
   * Select all links in the current view. 
   */
  selectAll: function() {
    this._activeView.selectAll();
  },

  /**
   * Loads a URL in the appropriate tab or window, given the user's preference
   * specified by modifier keys tracked by a DOM event
   * @param   event
   *          The DOM Mouse event with modifier keys set that track the user's
   *          preferred destination window or tab.
   */
  mouseLoadURI: function PC_mouseLoadURI(event) {
    var node = this._activeView.selectedURINode;
    if (node)
      this._activeView.browserWindow.openUILink(node.uri, event, false, false);
  },

  /**
   * Loads the selected URL in a new tab. 
   */
  openLinkInNewTab: function PC_openLinkInNewTab() {
    var node = this._activeView.selectedURINode;
    if (node)
      this._activeView.browserWindow.openNewTabWith(node.uri, null, null);
  },

  /**
   * Loads the selected URL in a new window.
   */
  openLinkInNewWindow: function PC_openLinkInNewWindow() {
    var node = this._activeView.selectedURINode;
    if (node)
      this._activeView.browserWindow.openNewWindowWith(node.uri, null, null);
  },

  /**
   * Loads the selected URL in the current window, replacing the Places page.
   */
  openLinkInCurrentWindow: function PC_openLinkInCurrentWindow() {
    var node = this._activeView.selectedURINode;
    if (node)
      this._activeView.browserWindow.loadURI(node.uri, null, null);
  },
  
  /**
   * Opens the links in the selected folder, or the selected links in new tabs. 
   */
  openLinksInTabs: function PC_openLinksInTabs() {
    var node = this._activeView.selectedNode;
    if (this._activeView.hasSingleSelection && this.nodeIsFolder(node)) {
      asFolder(node);
      var cc = node.childCount;
      for (var i = 0; i < cc; ++i) {
        var childNode = node.getChild(i);
        if (this.nodeIsURI(childNode))
          this._activeView.browserWindow.openNewTabWith(childNode.uri,
              null, null);
      }
    }
    else {
      var nodes = this._activeView.getSelectionNodes();
      for (var i = 0; i < nodes.length; ++i) {
        if (this.nodeIsURI(nodes[i]))
          this._activeView.browserWindow.openNewTabWith(nodes[i].uri,
              null, null);
      }
    }
  },
  
  /**
   * Loads the contents of a query node into a view with the specified grouping
   * and sort options.
   * @param   view
   *          The tree view to load the contents of the node into
   * @param   node
   *          The node to load the contents of
   * @param   groupings
   *          Groupings to be applied to the displayed contents, [] for no 
   *          grouping
   * @param   sortingMode
   *          The sorting mode to be applied to the displayed contents. 
   */
  loadNodeIntoView: function PC_loadQueries(view, node, groupings, sortingMode) {
    ASSERT(view, "Must have a view to load node contents into!");
    var node = asQuery(node);
    var queries = node.getQueries({ });
    var newQueries = [];
    for (var i = 0; i < queries.length; ++i) {
      var query = queries[i].clone();
      newQueries.push(query);
    }
    var newOptions = node.queryOptions.clone();
    
    // Update the grouping mode only after persisting, so that the URI is not 
    // changed. 
    newOptions.setGroupingMode(groupings, groupings.length);
    
    // Set the sort order of the results
    newOptions.sortingMode = sortingMode;
    
    // Reload the view 
    view.load(newQueries, newOptions);
  },
  
  
  /**
   * A hash of groupers that supply grouping options for queries of a given 
   * type. This is an override of grouping options that might be encoded in
   * a saved place: URI
   */
  groupers: { },
  
  /**
   * Rebuilds the view using a new set of grouping options.
   * @param   groupings
   *          An array of grouping options, see nsINavHistoryQueryOptions
   *          for details.
   * @param   sortingMode
   *          The type of sort that should be applied to the results.
   */
  setGroupingMode: function PC_setGroupingMode(groupings, sortingMode) {
    if (!this._groupableView)
      return;
    var node = this._groupableView.getResult().root;
    this.loadNodeIntoView(this._groupableView, node, groupings, sortingMode);

    // Persist this selection
    if (this._groupableView.isBookmarks && "bookmark" in this.groupers)
      this.groupers.bookmark.value = groupings;
    else if ("generic" in this.groupers)
      this.groupers.generic.value = groupings;
  },
  
  /**
   * Group the current content view by domain
   */
  groupBySite: function PC_groupBySite() {
    var groupings = [Ci.nsINavHistoryQueryOptions.GROUP_BY_DOMAIN];
    var sortMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING;
    this.setGroupingMode(groupings, sortMode);
  },
  
  /**
   * Group the current content view by folder
   */
  groupByFolder: function PC_groupByFolder() {
    var groupings = [Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER];
    var sortMode = Ci.nsINavHistoryQueryOptions.SORT_BY_NONE;
    this.setGroupingMode(groupings, sortMode);
  },
  
  /**
   * Ungroup the current content view (i.e. show individual pages)
   */
  groupByPage: function PC_groupByPage() {
    this.setGroupingMode([], 
      Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING);
  },
  
  /**
   * Loads all results matching a certain annotation, grouped and sorted as 
   * specified.
   * @param   annotation
   *          The annotation to match
   * @param   groupings
   *          The grouping to apply to the displayed results
   * @param   sortingMode
   *          The sorting to apply to the displayed results
   */
  groupByAnnotation: 
  function PC_groupByAnnotation(annotation, groupings, sortingMode) {
    ASSERT(this._groupableView, "Need a groupable view to load!");
    if (!this._groupableView)
      return null;
    var query = this._hist.getNewQuery();
    var options = this._hist.getNewQueryOptions();
    options.setGroupingMode(groupings, groupings.length);
    options.sortingMode = sortingMode;
    query.annotation = annotation;
    this.groupableView.load([query], options);
    LOG("CS: " + this._hist.queriesToQueryString([query], 1, options));
  },
  
  /**
   * Create a new Bookmark folder somewhere. Prompts the user for the name
   * of the folder. 
   */
  newFolder: function PC_newFolder() {
    var view = this._activeView;
    
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
      var txn = new PlacesCreateFolderTransaction(value.value, ip.folderId, 
                                                  ip.index);
      this.tm.doTransaction(txn);
      this._activeView.focus();
      view.restoreSelection();
    }
  },

  /**
   * Create a new Bookmark separator somewhere.
   */
  newSeparator: function PC_newSeparator() {
    var ip = this._activeView.insertionPoint;
    var txn = new PlacesInsertSeparatorTransaction(ip.folderId, ip.index);
    this.tm.doTransaction(txn);
    this._activeView.focus();
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
    ASSERT(transactions instanceof Array, "Must pass a transactions array");
    var index = this.getIndexOfNode(range[0]);
    
    // Walk backwards to preserve insertion order on undo
    for (var i = range.length - 1; i >= 0; --i) {
      var node = range[i];
      if (this.nodeIsFolder(node)) {
        // TODO -- node.parent might be a query and not a folder.  See bug 324948
        transactions.push(new PlacesRemoveFolderTransaction(
          asFolder(node).folderId, asFolder(node.parent).folderId, index));
      }
      else if (this.nodeIsSeparator(node)) {
        // A Bookmark separator.
        transactions.push(new PlacesRemoveSeparatorTransaction(
          asFolder(node.parent).folderId, index));
      }
      else if (this.nodeIsFolder(node.parent)) {
        // A Bookmark in a Bookmark Folder.
        transactions.push(new PlacesRemoveItemTransaction(
          this._uri(node.uri), asFolder(node.parent).folderId, index));
      }
    }
  },
  
  /**
   * Removes the set of selected ranges from bookmarks.
   * @param   txnName
   *          See |remove|.
   */
  _removeRowsFromBookmarks: function PC__removeRowsFromBookmarks(txnName) {
    var ranges = this._activeView.getRemovableSelectionRanges();
    var transactions = [];
    for (var i = ranges.length - 1; i >= 0 ; --i)
      this._removeRange(ranges[i], transactions);
    if (transactions.length > 0) {
      var txn = new PlacesAggregateTransaction(txnName, transactions);
      this.tm.doTransaction(txn);
    }
  },
  
  /**
   * Removes the set of selected ranges from history. 
   */
  _removeRowsFromHistory: function PC__removeRowsFromHistory() {
    // Other containers are history queries, just delete from history
    // history deletes are not undoable.
    var nodes = this._activeView.getSelectionNodes();
    for (i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      var bhist = this._hist.QueryInterface(Ci.nsIBrowserHistory);
      if (this.nodeIsHost(node))
        bhist.removePagesFromHost(node.title, true);
      else if (this.nodeIsURI(node))
        bhist.removePage(this._uri(node.uri));
    }
  },
   
  /**
   * Removes the selection
   * @param   txnName
   *          A name for the transaction if this is being performed
   *          as part of another operation.
   */
  remove: function PC_remove(txnName) {
    ASSERT(txnName !== undefined, "Must supply Transaction Name");
    this._activeView.saveSelection(this._activeView.SAVE_SELECTION_REMOVE);

    // Delete the selected rows. Do this by walking the selection backward, so
    // that when undo is performed they are re-inserted in the correct order.
    var type = this._activeView.getResult().root.type; 
    LOG("TYPE: " + type);
    if (type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER)
      this._removeRowsFromBookmarks(txnName);
    else
      this._removeRowsFromHistory();

    this._activeView.restoreSelection();
  },

  /**
   * Gets the index of a node within its parent container
   * @param   node
   *          The node to look up
   * @returns The index of the node within its parent container, or -1 if the
   *          node was not found or the node specified has no parent.
   */
  getIndexOfNode: function PC_getIndexOfNode(node) {
    var parent = node.parent;
    if (!parent || !this.nodeIsContainer(parent))
      return -1;
    var cc = asContainer(parent).childCount;
    for (var i = 0; i < cc && asContainer(parent).getChild(i) != node; ++i);
    return i < cc ? i : -1;
  },
  
  /**
   * String-wraps a NavHistoryResultNode according to the rules of the specified
   * content type.
   * @param   node
   *          The Result node to wrap (serialize)
   * @param   type
   *          The content type to serialize as
   * @returns A string serialization of the node
   */
  wrapNode: function PC_wrapNode(node, type) {
    switch (type) {
    case TYPE_X_MOZ_PLACE_CONTAINER:
    case TYPE_X_MOZ_PLACE: 
    case TYPE_X_MOZ_PLACE_SEPARATOR:
      // Data is encoded like this:
      // bookmarks folder: <folderId>\n<>\n<parentId>\n<indexInParent>
      // uri:              0\n<uri>\n<parentId>\n<indexInParent>
      // separator:        0\n<>\n<parentId>\n<indexInParent>
      var wrapped = "";
      if (this.nodeIsFolder(node))
        wrapped += asFolder(node).folderId + NEWLINE;
      else
        wrapped += "0" + NEWLINE;
        
      if (this.nodeIsURI(node))
        wrapped += node.uri + NEWLINE;
      else
        wrapped += NEWLINE;
        
      if (this.nodeIsFolder(node.parent))
        wrapped += asFolder(node.parent).folderId + NEWLINE;
      else
        wrapped += "0" + NEWLINE;
        
      wrapped += this.getIndexOfNode(node);
      return wrapped;
    case TYPE_X_MOZ_URL:
      return node.uri + NEWLINE + node.title;
    case TYPE_HTML:
      return "<A HREF=\"" + node.uri + "\">" + node.title + "</A>";
    }
    // case TYPE_UNICODE:
    return node.uri;
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
  unwrapNodes: function PC_unwrapNodes(blob, type) {
    // We use \n here because the transferable system converts \r\n to \n
    var parts = blob.split("\n");
    var nodes = [];
    for (var i = 0; i < parts.length; ++i) {
      var data = { };
      switch (type) {
      case TYPE_X_MOZ_PLACE_CONTAINER:
      case TYPE_X_MOZ_PLACE:
      case TYPE_X_MOZ_PLACE_SEPARATOR:
        nodes.push({  folderId: parseInt(parts[i++]),
                      uri: parts[i] ? this._uri(parts[i]) : null,
                      parent: parseInt(parts[++i]),
                      index: parseInt(parts[++i]) });
        break;
      case TYPE_X_MOZ_URL:
        nodes.push({  uri: this._uri(parts[i++]),
                      title: parts[i] });
        break;
      case TYPE_UNICODE:
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
   * Get a transaction for copying a leaf item from one container to another. 
   * @param   uri
   *          The URI of the item being copied
   * @param   container
   *          The container being copied into
   * @param   index
   *          The index within the container the item is copied to
   * @returns A nsITransaction object that performs the copy. 
   */
  _getItemCopyTransaction: function (uri, container, index) {
    var itemTitle = this._bms.getItemTitle(uri);
    var createTxn = new PlacesCreateItemTransaction(uri, container, index);
    var editTxn = new PlacesEditItemTransaction(uri, { title: itemTitle });
    return new PlacesAggregateTransaction("ItemCopy", [createTxn, editTxn]);
  },
  
  /**
   * Gets a transaction for copying (recursively nesting to include children)
   * a folder and its contents from one folder to another. 
   * @param   data
   *          Unwrapped dropped folder data
   * @param   container
   *          The container we are copying into
   * @param   index
   *          The index in the destination container to insert the new items
   * @returns A nsITransaction object that will perform the copy. 
   */
  _getFolderCopyTransaction: 
  function PC__getFolderCopyTransaction(data, container, index) {
    var transactions = [];
    var self = this;
    function createTransactions(folderId, container, index) {
      var folderTitle = self._bms.getFolderTitle(folderId);
    
      var createTxn = 
        new PlacesCreateFolderTransaction(folderTitle, container, index);
      transactions.push(createTxn);
    
      // Get the folder's children
      var kids = self.getFolderContents(folderId, false, false);
      var cc = kids.childCount;
      for (var i = 0; i < cc; ++i) {
        var node = kids.getChild(i);
        if (self.nodeIsFolder(node))
          createTransactions(node.folderId, folderId, i);
        else if (this.nodeIsURI(node)) {
          var uri = self._uri(node.uri);
          transactions.push(self._getItemCopyTransaction(uri, container, 
                                                         index));
        }
      }
    }
    createTransactions(data.folderId, container, index);
    return new PlacesAggregateTransaction("FolderCopy", transactions);
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
  makeTransaction: function PC_makeTransaction(data, type, container, 
                                               index, copy) {
    switch (type) {
    case TYPE_X_MOZ_PLACE_CONTAINER:
    case TYPE_X_MOZ_PLACE:
      if (data.folderId > 0) {
        // Place is a folder. 
        if (copy)
          return this._getFolderCopyTransaction(data, container, index);
        return new PlacesMoveFolderTransaction(data.folderId, data.parent, 
                                               data.index, container, 
                                               index);
      }
      if (copy)
        return this._getItemCopyTransaction(data.uri, container, index);
      return new PlacesMoveItemTransaction(data.uri, data.parent, 
                                           data.index, container, 
                                           index);
    case TYPE_X_MOZ_PLACE_SEPARATOR:
      if (copy) {
        // There is no data in a separator, so copying it just amounts to
        // inserting a new separator.
        return new PlacesInsertSeparatorTransaction(container, index);
      }
      // Similarly, moving a separator is just removing the old one and
      // then creating a new one.
      var removeTxn =
        new PlacesRemoveSeparatorTransaction(data.parent, data.index);
      var createTxn =
        new PlacesInsertSeparatorTransaction(container, index);
      return new PlacesAggregateTransaction("SeparatorMove", [removeTxn, createTxn]);
    case TYPE_X_MOZ_URL:
      // Creating and Setting the title is a two step process, so create
      // a transaction for each then aggregate them. 
      var createTxn = 
        new PlacesCreateItemTransaction(data.uri, container, index);
      var editTxn = 
        new PlacesEditItemTransaction(data.uri, { title: data.title });
      return new PlacesAggregateTransaction("DropMozURLItem", [createTxn, editTxn]);
    case TYPE_UNICODE:
      // Creating and Setting the title is a two step process, so create
      // a transaction for each then aggregate them. 
      var createTxn = 
        new PlacesCreateItemTransaction(data.uri, container, index);
      var editTxn = 
        new PlacesEditItemTransaction(data.uri, { title: data.uri });
      return new PlacesAggregateTransaction("DropItem", [createTxn, editTxn]);
    }
    return null;
  },
  
  /**
   * Wraps a string in a nsISupportsString wrapper
   * @param   str
   *          The string to wrap
   * @returns A nsISupportsString object containing a string.
   */
  _wrapString: function PC__wrapString(str) {
    var s = 
        Cc["@mozilla.org/supports-string;1"].
        createInstance(Ci.nsISupportsString);
    s.data = str;
    return s;
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
      nodes = this._activeView.getCopyableSelection();
    else
      nodes = this._activeView.getDragableSelection();
    var dataSet = new TransferDataSet();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      
      var data = new TransferData();
      var self = this;
      function addData(type) {
        data.addDataForFlavour(type, self._wrapString(self.wrapNode(node, type)));
      }
      
      if (this.nodeIsFolder(node) || this.nodeIsQuery(node)) {
        // Look up this node's place: URI in the annotation service to see if 
        // it is a special, non-movable folder. 
        // XXXben: TODO
        
        addData(TYPE_X_MOZ_PLACE_CONTAINER);
      }
      else if (this.nodeIsSeparator(node)) {
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
    var nodes = this._activeView.getCopyableSelection();
    
    var xferable = 
        Cc["@mozilla.org/widget/transferable;1"].
        createInstance(Ci.nsITransferable);
    var foundFolder = false, foundLink = false;
    var pcString = psString = placeString = mozURLString = htmlString = unicodeString = "";
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      var self = this;
      function generateChunk(type) {
        var suffix = i < (nodes.length - 1) ? NEWLINE : "";
        return self.wrapNode(node, type) + suffix;
      }
      if (this.nodeIsFolder(node) || this.nodeIsQuery(node)) 
        pcString += generateChunk(TYPE_X_MOZ_PLACE_CONTAINER);
      else if (this.nodeIsSeparator(node)) {
        psString += generateChunk(TYPE_X_MOZ_PLACE_SEPARATOR);
      }
      else {
        placeString += generateChunk(TYPE_X_MOZ_PLACE);
        mozURLString += generateChunk(TYPE_X_MOZ_URL);
        htmlString += generateChunk(TYPE_HTML);
        unicodeString += generateChunk(TYPE_UNICODE);
      }
    }
    
    var self = this;
    function addData(type, data) {
      xferable.addDataFlavor(type);
      xferable.setTransferData(type, self._wrapString(data), data.length * 2);
    }
    // This order is _important_! It controls how this and other applications 
    // select data to be inserted based on type. 
    if (pcString)
      addData(TYPE_X_MOZ_PLACE_CONTAINER, pcString);
    if (psString)
      addData(TYPE_X_MOZ_PLACE_SEPARATOR, psString);
    if (placeString)
      addData(TYPE_X_MOZ_PLACE, placeString);
    if (unicodeString)
      addData(TYPE_UNICODE, unicodeString);
    if (htmlString)
      addData(TYPE_HTML, htmlString);
    if (mozURLString)
      addData(TYPE_X_MOZ_URL, mozURLString);
    
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
  paste: function() {
    var xferable = 
        Cc["@mozilla.org/widget/transferable;1"].
        createInstance(Ci.nsITransferable);
    xferable.addDataFlavor(TYPE_X_MOZ_PLACE_CONTAINER);
    xferable.addDataFlavor(TYPE_X_MOZ_PLACE_SEPARATOR);
    xferable.addDataFlavor(TYPE_X_MOZ_PLACE);
    xferable.addDataFlavor(TYPE_X_MOZ_URL);
    xferable.addDataFlavor(TYPE_UNICODE);
    
    var clipboard = 
        Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    clipboard.getData(xferable, Ci.nsIClipboard.kGlobalClipboard);
    
    var data = { }, type = { };
    xferable.getAnyTransferData(type, data, { });
    data = data.value.QueryInterface(Ci.nsISupportsString).data;
    data = this.unwrapNodes(data, type.value);
    
    var ip = this._activeView.insertionPoint;
    var transactions = [];
    for (var i = 0; i < data.length; ++i)
      transactions.push(this.makeTransaction(data[i], type.value, 
                                             ip.folderId, ip.index, true));
    
    var txn = new PlacesAggregateTransaction("Paste", transactions);
    this.tm.doTransaction(txn);
  },
};

/**
 * Handles drag and drop operations for views. Note that this is view agnostic!
 * You should not use PlacesController.activeView within these methods, since
 * the view that the item(s) have been dropped on was not necessarily active. 
 * Drop functions are passed the view that is being dropped on. 
 */
var PlacesControllerDragHelper = {
  /**
   * DOM Element currently being dragged over
   */
  currentDropTarget: null,
   
  /**
   * @returns The current active drag session. Returns null if there is none.
   */
  getSession: function VO__getSession() {
    var dragService = 
        Cc["@mozilla.org/widget/dragservice;1"].
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
    if (PlacesController.nodeIsReadOnly(parent) || 
        !PlacesController.nodeIsFolder(parent))
      return false;
  
    var session = this.getSession();
    if (session) {
      if (orientation != NHRVO.DROP_ON)
        var types = view.supportedDropTypes;
      else
        types = view.supportedDropOnTypes;
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
   * @param   view
   *          An object implementing the AVI that supplies a list of 
   *          supported droppable content types
   * @param   orientation
   *          The orientation of the drop
   * @returns An object implementing nsITransferable that can receive data
   *          dropped onto a view. 
   */
  _initTransferable: function PCDH__initTransferable(view, orientation) {
    var xferable = 
        Cc["@mozilla.org/widget/transferable;1"].
        createInstance(Ci.nsITransferable);
    if (orientation != NHRVO.DROP_ON) 
      var types = view.supportedDropTypes;
    else
      types = view.supportedDropOnTypes;    
    for (var j = 0; j < types.length; ++j)
      xferable.addDataFlavor(types[j]);
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
   * @param   visibleInsertCount
   *          The number of visible items to be inserted. This can be zero
   *          even when items are dropped because this is how many items will
   *          be _visible_ in the resulting tree. 
   *          XXXben this parameter appears to be unused! check call sites.
   *                 check d&d feedback.
   */
  onDrop: function PCDH_onDrop(sourceView, targetView, insertionPoint, 
                               visibleInsertCount) {
    var session = this.getSession();
    var copy = session.dragAction & Ci.nsIDragService.DRAGDROP_ACTION_COPY;
    var transactions = [];
    var xferable = this._initTransferable(targetView, 
                                          insertionPoint.orientation);
    var dropCount = session.numDropItems;
    for (var i = dropCount - 1; i >= 0; --i) {
      session.getData(xferable, i);
    
      var data = { }, flavor = { };
      xferable.getAnyTransferData(flavor, data, { });
      data.value.QueryInterface(Ci.nsISupportsString);
      
      // There's only ever one in the D&D case. 
      var unwrapped = PlacesController.unwrapNodes(data.value.data, 
                                                   flavor.value)[0];
      transactions.push(PlacesController.makeTransaction(unwrapped, 
                        flavor.value, insertionPoint.folderId, 
                        insertionPoint.index, copy));
    }
    
    var txn = new PlacesAggregateTransaction("DropItems", transactions);
    PlacesController.tm.doTransaction(txn);
  }
};

/**
 * Method and utility stubs for Place Edit Transactions
 */
function PlacesBaseTransaction() {
}
PlacesBaseTransaction.prototype = {
  _bms: Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
        getService(Ci.nsINavBookmarksService), 
           
  redoTransaction: function PIT_redoTransaction() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  
  pageTransaction: false,
  
  get isTransient() {
    return false;
  },
  
  merge: function PIT_merge(transaction) {
    return false;
  },
};

/**
 * Performs several Places Transactions in a single batch. 
 */
function PlacesAggregateTransaction(name, transactions) {
  this._transactions = transactions;
  this._name = name;
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesAggregateTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,
  
  doTransaction: function() {
    LOG("== " + this._name + " (Aggregate) ==============");
    this._bms.beginUpdateBatch();
    for (var i = 0; i < this._transactions.length; ++i)
      this._transactions[i].doTransaction();
    this._bms.endUpdateBatch();
    LOG("== " + this._name + " (Aggregate Ends) =========");
  },
  
  undoTransaction: function() {
    LOG("== UN" + this._name + " (UNAggregate) ============");
    this._bms.beginUpdateBatch();
    for (var i = 0; i < this._transactions.length; ++i)
      this._transactions[i].undoTransaction();
    this._bms.endUpdateBatch();
    LOG("== UN" + this._name + " (UNAggregate Ends) =======");
  },
};


/**
 * Create a new Folder
 */
function PlacesCreateFolderTransaction(name, container, index) {
  this._name = name;
  this._container = container;
  this._index = index;
  this._id = null;
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesCreateFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PCFT_doTransaction() {
    LOG("Create Folder: " + this._name + " in: " + this._container + "," + this._index);
    this._id = this._bms.createFolder(this._container, this._name, this._index);
  },
  
  undoTransaction: function PCFT_undoTransaction() {
    LOG("UNCreate Folder: " + this._name + " from: " + this._container + "," + this._index);
    this._bms.removeFolder(this._id);
  },
};

/**
 * Create a new Item
 */
function PlacesCreateItemTransaction(uri, container, index) {
  this._uri = uri;
  this._container = container;
  this._index = index;
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesCreateItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PCIT_doTransaction() {
    LOG("Create Item: " + this._uri.spec + " in: " + this._container + "," + this._index);
    this._bms.insertItem(this._container, this._uri, this._index);
  },
  
  undoTransaction: function PCIT_undoTransaction() {
    LOG("UNCreate Item: " + this._uri.spec + " from: " + this._container + "," + this._index);
    this._bms.removeItem(this._container, this._uri);
  },
};

/**
 * Create a new Separator
 */
function PlacesInsertSeparatorTransaction(container, index) {
  this._container = container;
  this._index = index;
  this._id = null;
}
PlacesInsertSeparatorTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PIST_doTransaction() {
    LOG("Create separator in: " + this._container + "," + this._index);
    this._id = this._bms.insertSeparator(this._container, this._index);
  },
  
  undoTransaction: function PIST_undoTransaction() {
    LOG("UNCreate separator from: " + this._container + "," + this._index);
    this._bms.removeChildAt(this._container, this._index);
  },
};

/**
 * Move a Folder
 */
function PlacesMoveFolderTransaction(id, oldContainer, oldIndex, newContainer, newIndex) {
  ASSERT(!isNaN(id + oldContainer + oldIndex + newContainer + newIndex), "Parameter is NaN!");
  this._id = id;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this._newContainer = newContainer;
  this._newIndex = newIndex;
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesMoveFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PMFT_doTransaction() {
    LOG("Move Folder: " + this._id + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this._bms.moveFolder(this._id, this._newContainer, this._newIndex);
  },
  
  undoTransaction: function PMFT_undoTransaction() {
    LOG("UNMove Folder: " + this._id + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this._bms.moveFolder(this._id, this._oldContainer, this._oldIndex);
  },
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
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesMoveItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype,

  doTransaction: function PMIT_doTransaction() {
    LOG("Move Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this._bms.removeItem(this._oldContainer, this._uri);
    this._bms.insertItem(this._newContainer, this._uri, this._newIndex);
  },
  
  undoTransaction: function PMIT_undoTransaction() {
    LOG("UNMove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex + " to: " + this._newContainer + "," + this._newIndex);
    this._bms.removeItem(this._newContainer, this._uri);
    this._bms.insertItem(this._oldContainer, this._uri, this._oldIndex);
  },
};

/**
 * A named leaf item.
 * @param   name
 *          The name of the item
 * @param   uri
 *          The URI fo the item
 */
function PlacesRemoveFolderSaveChildItem(name, uri) {
  this.name = name;
  var ios = 
      Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);  
  this.uri = ios.newURI(uri, null, null);
}
/**
 * A named folder, with children.
 * @param   name
 *          The name of the folder.
 */
function PlacesRemoveFolderSaveChildFolder(name) {
  this.name = name;
  this.children = [];
} 
/**
 * Remove a Folder
 * This is a little complicated. When we remove a container we need to remove 
 * all of its children. We can't just repurpose our existing transactions for
 * this since they cache their parent container id. Since the folder structure
 * is being removed, this id is being destroyed and when it is re-created will
 * likely have a different id.
 */

function PlacesRemoveFolderTransaction(id, oldContainer, oldIndex) {
  this._id = id;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this._oldFolderTitle = null;
  this._contents = null; // The encoded contents of this folder
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesRemoveFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 
  
  /**
   * Save the contents of a folder (items and containers) for restoration 
   * purposes later.
   * @param   id
   *          The id of the folder
   * @param   parent
   *          The parent PlacesRemoveFolderSaveChildFolder object
   */
  _saveFolderContents: function PRFT__saveFolderContents(id, parent) {
    var contents = PlacesController.getFolderContents(id, false, false);
    for (var i = contents.childCount - 1; i >= 0; --i) {
      var child = contents.getChild(i);
      var obj = null;
      if (child.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER) {
        obj = new PlacesRemoveFolderSaveChildFolder(child.title);
        parent.children.push(obj);
        this._saveFolderContents(asFolder(child).folderId, obj);
      }
      else {
        obj = new PlacesRemoveFolderSaveChildItem(child.title, child.uri);
        parent.children.push(obj);
      }
    }
  },
  
  /**
   * Recreate a folder hierarchy from a saved data set.
   * @param   parent
   *          The id of the folder to create the items beneath
   * @param   item
   *          The object that contains the saved data.
   */
  _restore: function PRFT__restore(parent, item) {
    if (item instanceof PlacesRemoveFolderSaveChildFolder) {
      var id = this._bms.createFolder(parent, item.name, 0);
      this._restore(id, item);
    }
    else {
      this._bms.insertItem(parent, item.uri, 0);
      this._bms.setItemTitle(item.uri, item.name);
    }    
  },

  doTransaction: function PRFT_doTransaction() {
    this._oldFolderTitle = this._bms.getFolderTitle(this._id);
    LOG("Remove Folder: " + this._oldFolderTitle + " from: " + this._oldContainer + "," + this._oldIndex);
    
    this._contents = new PlacesRemoveFolderSaveChildFolder(this._oldFolderTitle);
    this._saveFolderContents(this._id, this._contents);

    this._bms.removeFolder(this._id);
  },
  
  undoTransaction: function PRFT_undoTransaction() {
    LOG("UNRemove Folder: " + this._oldFolderTitle + " from: " + this._oldContainer + "," + this._oldIndex);
    this._id = this._bms.createFolder(this._oldContainer, this._oldFolderTitle, this._oldIndex);
    
    for (var i = 0; i < this._contents.children.length; ++i)
      this._restore(this._id, this._contents.children[i]);
  },
};

/**
 * Remove an Item
 */
function PlacesRemoveItemTransaction(uri, oldContainer, oldIndex) {
  this._uri = uri;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesRemoveItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 
  
  doTransaction: function PRIT_doTransaction() {
    LOG("Remove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex);
    this._bms.removeItem(this._oldContainer, this._uri);
    LOG("DO: PAGETXN: " + this.pageTransaction);
  },
  
  undoTransaction: function PRIT_undoTransaction() {
    LOG("UNRemove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex);
    this._bms.insertItem(this._oldContainer, this._uri, this._oldIndex);
    LOG("UNDO: PAGETXN: " + this.pageTransaction);
  },
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
    LOG("Remove Separator from: " + this._oldContainer + "," + this._oldIndex);
    this._bms.removeChildAt(this._oldContainer, this._oldIndex);
  },
  
  undoTransaction: function PRST_undoTransaction() {
    LOG("UNRemove Separator from: " + this._oldContainer + "," + this._oldIndex);
    this._bms.insertSeparator(this._oldContainer, this._oldIndex);
  },
};

/**
 * Edit a Folder
 */
function PlacesEditFolderTransaction(id, oldAttributes, newAttributes) {
  this._id = id;
  this._oldAttributes = oldAttributes;
  this._newAttributes = newAttributes;
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesEditFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 

  doTransaction: function PEFT_doTransaction() {
    LOG("Edit Folder: " + this._id + " oldAttrs: " + this._oldAttributes.toSource() + " newAttrs: " + this._newAttributes.toSource());
    // Use Bookmarks and Annotation Services to perform these operations.
  },
  
  undoTransaction: function PEFT_undoTransaction() {
    LOG("UNEdit Folder: " + this._id + " oldAttrs: " + this._oldAttributes.toSource() + " newAttrs: " + this._newAttributes.toSource());
    // Use Bookmarks and Annotation Services to perform these operations.
  },
};

/**
 * Edit an Item
 */
function PlacesEditItemTransaction(uri, newAttributes) {
  this._uri = uri;
  this._newAttributes = newAttributes;
  this._oldAttributes = { };
  this.redoTransaction = this.doTransaction;
  this.pageTransaction = PlacesController.activeView.filterTransactions;
  ASSERT(this.pageTransaction !== undefined, "Don't know if this transaction must be filtered");
}
PlacesEditItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 
  
  doTransaction: function PEIT_doTransaction() {
    LOG("Edit Item: " + this._uri.spec + " oldAttrs: " + this._oldAttributes.toSource() + " newAttrs: " + this._newAttributes.toSource());
    for (var p in this._newAttributes) {
      if (p == "title") {
        this._oldAttributes[p] = this._bms.getItemTitle(this._uri);
        this._bms.setItemTitle(this._uri, this._newAttributes[p]);
      }
      else {
        // Use Annotation Service
      }
    }
  },
  
  undoTransaction: function PEIT_undoTransaction() {
    LOG("UNEdit Item: " + this._uri.spec + " oldAttrs: " + this._oldAttributes.toSource() + " newAttrs: " + this._newAttributes.toSource());
    for (var p in this._newAttributes) {
      if (p == "title")
        this._bms.setItemTitle(this._uri, this._oldAttributes[p]);
      else {
        // Use Annotation Service
      }
    }
  },
};
/*
 
 AVI rules:
 
 readonly attribute boolean hasSelection;
 readonly attribute boolean hasSingleSelection;
 readonly attribute boolean selectionIsContainer;
 readonly attribute boolean containerIsOpen;
 void getSelectedNodes([retval, array, size_is(nodeCount)] out nodes, out nodeCount);
 
 selection flags

 flags:
   SELECTION_CONTAINS_URI
   SELECTION_CONTAINS_CONTAINER_OPEN
   SELECTION_CONTAINS_CONTAINER_CLOSED
   SELECTION_CONTAINS_CHANGEABLE
   SELECTION_CONTAINS_REMOVABLE
   SELECTION_CONTAINS_MOVABLE 
 
 Given a:
   - view, via AVI
   - query
   - query options
   
 Determine the state of commands!

*/
