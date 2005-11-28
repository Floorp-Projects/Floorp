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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

const SELECTION_CONTAINS_URL = 0x01;
const SELECTION_CONTAINS_CONTAINER = 0x02;
const SELECTION_IS_OPEN_CONTAINER = 0x04;
const SELECTION_IS_CLOSED_CONTAINER = 0x08;
const SELECTION_IS_CHANGEABLE = 0x10;
const SELECTION_IS_REMOVABLE = 0x20;
const SELECTION_IS_MOVABLE = 0x40;

// Place entries that are containers, e.g. bookmark folders or queries. 
const TYPE_X_MOZ_PLACE_CONTAINER = "text/x-moz-place-container";
// Place entries that are not containers
const TYPE_X_MOZ_PLACE = "text/x-moz-place";
// Place entries in shortcut url format (url\ntitle)
const TYPE_X_MOZ_URL = "text/x-moz-url";
// Place entries formatted as HTML anchors
const TYPE_HTML = "text/html";
// Place entries as raw URL text
const TYPE_UNICODE = "text/unicode";

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
 * @constructor
 */
function InsertionPoint(folderId, index) {
  this.folderId = folderId;
  this.index = index;
}

/**
 * The Master Places Controller
 */
var PlacesController = {
  _uri: function PC__uri(spec) {
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    return ios.newURI(spec, null, null);
  },
  
  /**
   * The Global Transaction Manager
   * XXXben - this needs to move into a service, because it ain't global!
   */
  __txmgr: null,
  get _txmgr() {
    if (!this.__txmgr) {
      this.__txmgr = 
        Cc["@mozilla.org/transactionmanager;1"].
        createInstance(Ci.nsITransactionManager);
    }
    return this.__txmgr;
  },
  
  __bms: null,
  get _bms() {
    if (!this.__bms) {
      this.__bms = 
        Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
        getService(Ci.nsINavBookmarksService);
    }
    return this.__bms;
  },

  _activeView: null,
  get activeView() {
    return this._activeView;
  },
  set activeView(activeView) {
    this._activeView = activeView;
    return this._activeView;
  },
  
  isCommandEnabled: function PC_isCommandEnabled(command) {
    //LOG("isCommandEnabled: " + command);
    return document.getElementById(command).getAttribute("disabled") == "true";
  },

  supportsCommand: function PC_supportsCommand(command) {
    //LOG("supportsCommand: " + command);
    return document.getElementById(command) != null;
  },

  doCommand: function PC_doCommand(command) {
    LOG("doCommand: " + command);
  },

  onEvent: function PC_onEvent(eventName) {
    LOG("onEvent: " + eventName);
  },
  
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
    var nodes = this._activeView.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      if (nodes[i].readonly) 
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
   * Determines whether or not the paste command is enabled, based on the 
   * content of the clipboard and the selection within the active view.
   */
  _canPaste: function PC__canPaste() {
    // XXXben: check selection to see if insertion point would suggest pasting 
    //         into an immutable container. 
    // XXXben: check clipboard for leaf data when pasting into views that can
    //         only take non-leaf data. This is going to be hard because the
    //         clipboard apis are teh suck. 
    return this._hasClipboardData();
  },
  
  /**
   * Determines whether or not a ResultNode is a Bookmark folder or not. 
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a Bookmark folder, false otherwise
   */
  nodeIsFolder: function PC_nodeIsFolder(node) {
    if (!node)
      STACK(arguments);
    return node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER;
  },
  
  /**
   * Determines whether or not a ResultNode is a URL item or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a URL item, false otherwise
   */
  nodeIsURL: function PC_nodeIsURL(node) {
    const NHRN = Ci.nsINavHistoryResultNode;
    return node.type == NHRN.RESULT_TYPE_URL || 
           node.type == NHRN.RESULT_TYPE_VISIT;
  },
  
  /**
   * Determines whether or not a ResultNode is a Query item or not
   * @param   node
   *          A NavHistoryResultNode
   * @returns true if the node is a Query item, false otherwise
   */
  nodeIsQuery: function PC_nodeIsQuery(node) {
    node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_QUERY;
  },
  
  onCommandUpdate: function PC_onCommandUpdate() {
    if (!this._activeView) {
      // Initial command update, no view yet. 
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
    var hasSelectedURL = this._activeView.selectedURLNode != null;
    this._setEnabled("placesCmd_open", hasSelectedURL);
    this._setEnabled("placesCmd_open:window", hasSelectedURL);
    this._setEnabled("placesCmd_open:tab", hasSelectedURL);
    
    // We can open multiple links in tabs if there is either: 
    //  a) a single folder selected
    //  b) many links or folders selected
    var singleFolderSelected = hasSingleSelection && 
      this.nodeIsFolder(this._activeView.selectedNode);
    this._setEnabled("placesCmd_open:tabs", 
      singleFolderSelected || !hasSingleSelection);
      
    var viewIsFolder = this.nodeIsFolder(this._activeView.getResult());
    // Persistent Sort
    this._setEnabled("placesCmd_sortby:name", viewIsFolder);
    // New Folder
    this._setEnabled("placesCmd_new:folder", viewIsFolder);
    // New Separator
    // ...
    this._setEnabled("placesCmd_new:separator", false);
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
   *    is-livemark   "livemark"
   * @returns an object with each of the properties above set if the selection
   *          matches that rule. 
   */
  _buildSelectionMetadata: function PC__buildSelectionMetadata() {
    var metadata = { mixed: true };
    
    var hasSingleSelection = this._activeView.hasSingleSelection;
    if (this._activeView.selectedURLNode && hasSingleSelection)
      metadata["link"] = true;
    var selectedNode = this._activeView.selectedNode;
    if (this.nodeIsFolder(selectedNode) && hasSingleSelection)
      metadata["folder"] = true;
    
    var foundNonLeaf = false;
    var nodes = this._activeView.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      if (node.type != Ci.nsINavHistoryResultNode.RESULT_TYPE_URL)
        foundNonLeaf = true;
      // XXXben check for livemarkness
      if (!node.readonly)
        metadata["mutable"] = true;
    }
    if (this._activeView.getAttribute("seltype") != "single")
      metadata["multiselect"] = true;
    if (!foundNonLeaf && nodes.length > 1)
      metadata["links"] = true;
    return metadata;
  },
  
  _shouldShowMenuItem: function(metadata, rules) {
    for (var i = 0; i < rules.length; ++i) {
      if (rules[i] in metadata)
        return true;
    }
    return false;
  },
    
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
   * Given a Mouse event, determine which function should be used to load
   * the selected link. Modifiers may override the default settings and cause
   * a link to be opened in a new tab or window. 
   * @param   event
   *          The DOM Mouse Event triggering a URL load
   * @returns A function which should be used to load the selected URL.
   */
  _getLoadFunctionForEvent: function PP__getLoadFunctionForEvent(event) {
    if (event.button != 0)
      return null;
    
    if (event.ctrlKey)
      return this.openLinkInNewTab;
    else if (event.shiftKey)
      return this.openLinkInNewWindow;
    return this.openLinkInCurrentWindow;
  },

  /**
   * Loads a URL in the appropriate tab or window, given the user's preference
   * specified by modifier keys tracked by a DOM event
   * @param   event
   *          The DOM Mouse event with modifier keys set that track the user's
   *          preferred destination window or tab.
   */
  mouseLoadURI: function PC_mouseLoadURI(event) {
    var fn = this._getLoadFunctionForEvent(event);
    if (fn)
      this._getLoadFunctionForEvent(event)();
  },

  /**
   * Loads the selected URL in a new tab. 
   */
  openLinkInNewTab: function PC_openLinkInNewTab() {
    var node = this._activeView.selectedURLNode;
    if (node)
      this._activeView.browserWindow.openNewTabWith(node.url, null, null);
  },

  /**
   * Loads the selected URL in a new window.
   */
  openLinkInNewWindow: function PC_openLinkInNewWindow() {
    var node = this._activeView.selectedURLNode;
    if (node)
      this._activeView.browserWindow.openNewWindowWith(node.url, null, null);
  },

  /**
   * Loads the selected URL in the current window, replacing the Places page.
   */
  openLinkInCurrentWindow: function PC_openLinkInCurrentWindow() {
    var node = this._activeView.selectedURLNode;
    if (node)
      this._activeView.browserWindow.loadURI(node.url, null, null);
  },
  
  /**
   * Opens the links in the selected folder, or the selected links in new tabs. 
   */
  openLinksInTabs: function PC_openLinksInTabs() {
    var node = this._activeView.selectedNode;
    if (this._activeView.hasSingleSelection && this.nodeIsFolder(node)) {
      var kids = this._bms.getFolderChildren(node.folderId, 
                                             this._bms.ITEM_CHILDREN);
      var cc = kids.childCount;
      for (var i = 0; i < cc; ++i)
        this._activeView.browserWindow.openNewTabWith(kids.getChild(i).url, 
                                                      null, null);
    }
    else {
      var nodes = this._activeView.getSelectionNodes();
      for (var i = 0; i < nodes.length; ++i) {
        if (this.nodeIsURL(nodes[i]))
          this._activeView.browserWindow.openNewTabWith(nodes[i].url, null, null);
      }
    }
  },
  
  /**
   * Rebuilds the view using a new set of grouping options.
   * @param   options
   *          An array of grouping options, see nsINavHistoryQueryOptions
   *          for details.
   */
  setGroupingMode: function PC_setGroupingOptions(options) {
    var result = this._activeView.view.QueryInterface(Ci.nsINavHistoryResult);
    var queries = result.getSourceQueries({ });
    var newOptions = result.sourceQueryOptions.clone();
    newOptions.setGroupingMode(options, options.length);
    
    this._activeView.load(queries, newOptions);
  },
  
  /**
   * Group the current content view by domain
   */
  groupBySite: function PC_groupBySite() {
    var modes = [Ci.nsINavHistoryQueryOptions.GROUP_BY_DOMAIN, 
    Ci.nsINavHistoryQueryOptions.GROUP_BY_HOST];
    this.setGroupingMode(modes);
  },
  
  /**
   * Ungroup the current content view (i.e. show individual pages)
   */
  groupByPage: function PC_groupByPage() {
    this.setGroupingMode([]);
  },
  
  /**
   * Create a new Bookmark folder somewhere. Prompts the user for the name
   * of the folder. 
   */
  newFolder: function PC_newFolder() {
    var view = this._activeView;
    
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
      this._txmgr.doTransaction(txn);
    }
  },
  
  /**
   * Removes the selection
   * @param   txnName
   *          An optional name for the transaction if this is being performed
   *          as part of another operation.
   */
  remove: function PC_remove(txnName) {
    var nodes = this._activeView.getSelectionNodes();
    var txns = [];
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      var index = this.getIndexOfNode(node);
      if (node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER) {
        txns.push(new PlacesRemoveFolderTransaction(node.folderId, 
                                                    node.parent.folderId, 
                                                    index));
      }
      else {
        txns.push(new PlacesRemoveItemTransaction(this._uri(node.url),
                                                  node.parent.folderId,
                                                  index));
      }
    }
    var txn = new PlacesAggregateTransaction(txnName || "RemoveItems", txns);
    this._txmgr.doTransaction(txn);
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
    if (!parent)
      return -1;
    var cc = parent.childCount;
    for (var i = 0; i < cc && parent.getChild(i) != node; ++i);
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
      return node.folderId + "\n" + node.url + "\n" + node.parent.folderId + "\n" + this.getIndexOfNode(node);
    case TYPE_X_MOZ_URL:
      return node.url + "\n" + node.title;
    case TYPE_HTML:
      return "<A HREF=\"" + node.url + "\">" + node.title + "</A>";
    }
    // case TYPE_UNICODE:
    return node.url;
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
    var parts = blob.split("\n");
    var nodes = [];
    for (var i = 0; i < parts.length; ++i) {
      var data = { };
      switch (type) {
      case TYPE_X_MOZ_PLACE_CONTAINER:
      case TYPE_X_MOZ_PLACE:
        nodes.push({  folderId: parseInt(parts[i++]),
                      uri: parts[i] ? this._uri(parts[i++]) : null,
                      parent: parseInt(parts[i++]),
                      index: parseInt(parts[i]) });
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
   * @retunrs An object implementing nsITransaction that can perform
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
          return this._getFolderCopyTransaction(data.folderId, container, index);
        return new PlacesMoveFolderTransaction(data.folderId, data.parent, 
                                               data.index, container, 
                                               index);
      }
      if (copy)
        return this._getItemCopyTransaction(data.uri, container, index);
      return new PlacesMoveItemTransaction(data.uri, data.parent, 
                                           data.index, container, 
                                           index);
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
   * @returns A TransferDataSet object that can be dragged and dropped 
   *          elsewhere.
   */
  getTransferData: function PC_getTransferData() {
    var nodes = this._activeView.getCopyableSelection();
    var dataSet = new TransferDataSet();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
              
      var data = new TransferData();
      var self = this;
      function addData(type) {
        data.addDataForFlavour(type, self._wrapString(self.wrapNode(node, type)));
      }
      if (this.nodeIsFolder(node) || this.nodeIsQuery(node))
        addData(TYPE_X_MOZ_PLACE_CONTAINER);
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
    var pcString = placeString = mozURLString = htmlString = unicodeString = "";
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      var self = this;
      function generateChunk(type) {
        var suffix = i < (nodes.length - 1) ? "\n" : "";
        return self.wrapNode(node, type) + suffix;
      }
      if (this.nodeIsFolder(node) || this.nodeIsQuery(node)) 
        pcString += generateChunk(TYPE_X_MOZ_PLACE_CONTAINER);
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
    if (placeString)
      addData(TYPE_X_MOZ_PLACE, placeString);
    if (unicodeString)
      addData(TYPE_UNICODE, unicodeString);
    if (htmlString)
      addData(TYPE_HTML, htmlString);
    if (mozURLString)
      addData(TYPE_X_MOZ_URL, mozURLString);
    
    if (pcString || placeString || unicodeString || htmlString || 
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
    this.remove("Cut");
  },
  
  /**
   * Paste Bookmarks and Folders from the clipboard
   */
  paste: function() {
    var xferable = 
        Cc["@mozilla.org/widget/transferable;1"].
        createInstance(Ci.nsITransferable);
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
    this._txmgr.doTransaction(txn);
  },
};


var PlacesControllerDragHelper = {
  /**
   * @returns The current active drag session. Returns null if there is none.
   */
  _getSession: function VO__getSession() {
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
    var session = this._getSession();
    if (session) {
      if (orientation != Ci.nsINavHistoryResultViewObserver.DROP_ON)
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
    function createTransactions(folderId, container, index) {
      var bms = PlacesController._bms;
      var folderTitle = bms.getFolderTitle(folderId);
    
      var createTxn = 
        new PlacesCreateFolderTransaction(folderTitle, container, index);
      transactions.push(createTxn);
    
      var kids = bms.getFolderChildren(folderId, bms.ALL_CHILDREN);
      var cc = kids.childCount;
      for (var i = 0; i < cc; ++i) {
        var node = kids.getChild(i);
        if (node.type == Ci.nsINavHistoryResultNode.RESULT_TYPE_FOLDER)
          createTransactions(node.folderId, folderId, i);
        else {
          var uri = PlacesController._uri(node.url);
          transactions.push(this._getItemCopyTransaction(uri, container, 
                                                         index));
        }
      }
    }
    createTransactions(data.id, container, index);
    return new PlacesAggregateTransaction("FolderCopy", transactions);
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
    if (orientation != Ci.nsINavHistoryResultViewObserver.DROP_ON) 
      var types = view.supportedDropTypes;
    else
      types = view.supportedDropOnTypes;    
    for (var j = 0; j < types.length; ++j)
      xferable.addDataFlavor(types[j]);
    return xferable;
  },
  
  /**
   * Handles the drop of one or more items onto a view.
   * @param   view
   *          The AVI-implementing object that received the drop. 
   * @param   container
   *          The container the drop was into
   * @param   index
   *          The index within the container the item was dropped at
   * @param   orientation
   *          The orientation of the drop
   */
  onDrop: function PCDH_onDrop(view, container, index, orientation) {
    var session = this._getSession();
    if (!session)
      return;
    
    var copy = session.dragAction & Ci.nsIDragService.DRAGDROP_ACTION_COPY;
    var transactions = [];
    var xferable = this._initTransferable(view, orientation);
    for (var i = 0; i < session.numDropItems; ++i) {
      session.getData(xferable, i);
    
      var data = { }, flavor = { };
      xferable.getAnyTransferData(flavor, data, { });
      data.value.QueryInterface(Ci.nsISupportsString);
      
      // There's only ever one in the D&D case. 
      var unwrapped = PlacesController.unwrapNodes(data.value.data, 
                                                   flavor.value)[0];
      transactions.push(PlacesController.makeTransaction(unwrapped, 
                        flavor.value, container, index, copy));
    }
    var txn = new PlacesAggregateTransaction("DropItems", transactions);
    PlacesController._txmgr.doTransaction(txn);
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
 * Move a Folder
 */
function PlacesMoveFolderTransaction(id, oldContainer, oldIndex, newContainer, newIndex) {
  this._id = id;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this._newContainer = newContainer;
  this._newIndex = newIndex;
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
 * Remove a Folder
 */
function PlacesRemoveFolderTransaction(id, oldContainer, oldIndex) {
  this._id = id;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
  this._oldFolderTitle = null;
}
PlacesRemoveFolderTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 

  doTransaction: function PRFT_doTransaction() {
    LOG("Remove Folder: " + this._id + " from: " + this._oldContainer + "," + this._oldIndex);
    this._oldFolderTitle = this._bms.getFolderTitle(this._id);
    this._bms.removeFolder(this._id);
  },
  
  undoTransaction: function PRFT_undoTransaction() {
    LOG("UNRemove Folder: " + this._id + " from: " + this._oldContainer + "," + this._oldIndex);
    this._id = this._bms.createFolder(this._oldContainer, this._oldFolderTitle, this._oldIndex);
  },
};

/**
 * Remove an Item
 */
function PlacesRemoveItemTransaction(uri, oldContainer, oldIndex) {
  this._uri = uri;
  this._oldContainer = oldContainer;
  this._oldIndex = oldIndex;
}
PlacesRemoveItemTransaction.prototype = {
  __proto__: PlacesBaseTransaction.prototype, 

  doTransaction: function PRIT_doTransaction() {
    LOG("Remove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex);
    this._bms.removeItem(this._oldContainer, this._uri);
  },
  
  undoTransaction: function PRIT_undoTransaction() {
    LOG("UNRemove Item: " + this._uri.spec + " from: " + this._oldContainer + "," + this._oldIndex);
    this._bms.insertItem(this._oldContainer, this._uri, this._oldIndex);
  },
};

/**
 * Edit a Folder
 */
function PlacesEditFolderTransaction(id, oldAttributes, newAttributes) {
  this._id = id;
  this._oldAttributes = oldAttributes;
  this._newAttributes = newAttributes;
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
   SELECTION_CONTAINS_URL
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


