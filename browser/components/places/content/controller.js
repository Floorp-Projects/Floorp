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

var PlacesController = {
  _activeView: null,
  get activeView() {
    return this._activeView;
  },
  set activeView(activeView) {
    this._activeView = activeView;
    return this._activeView;
  },
  
  isCommandEnabled: function PC_isCommandEnabled(command) {
    LOG("isCommandEnabled: " + command);
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
  
  buildContextMenu: function PC_buildContextMenu(popup) {
    if (document.popupNode.hasAttribute("view")) {
      var view = document.popupNode.getAttribute("view");
      this.activeView = document.getElementById(view);
    }
    
    // Determine availability/enabled state of commands
    // ...
    return true;
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
    this._getLoadFunctionForEvent(event)();
  },

  /**
   * Loads the selected URL in a new tab. 
   */
  openLinkInNewTab: function PC_openLinkInNewTab() {
    var view = this._activeView;
    view.browserWindow.openNewTabWith(view.selectedURLNode.url, null, null);
  },

  /**
   * Loads the selected URL in a new window.
   */
  openLinkInNewWindow: function PC_openLinkInNewWindow() {
    var view = this._activeView;
    view.browserWindow.openNewWindowWith(view.selectedURLNode.url, null, null);
  },

  /**
   * Loads the selected URL in the current window, replacing the Places page.
   */
  openLinkInCurrentWindow: function PC_openLinkInCurrentWindow() {
    var view = this._activeView;
    view.browserWindow.loadURI(view.selectedURLNode.url, null, null);
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
      LOG("Insertion Point = " + ip.toSource());
      
      var bms = 
          Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
          getService(Ci.nsINavBookmarksService);
      LOG("Insert Folder: " + value.value + " into: " + ip.container + " at: " + ip.index);
      var folder = bms.createFolder(ip.container, value.value, ip.index);
    }
  },
  
  /**
   * Removes the selection
   */
  remove: function() {
    var bms = 
        Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
        getService(Ci.nsINavBookmarksService);
    var ios = 
        Cc["@mozilla.org/network/io-service;1"].
        getService(Ci.nsIIOService);
    var nodes = this._activeView.getSelectionNodes();
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      if (node.folderId) {
        LOG("Remove Folder: " + node.folderId);
        bms.removeFolder(node.folderId);
      }
      else {
        LOG("Remove: " + node.url + " from: " + node.parent.folderId);
        bms.removeItem(node.parent.folderId, ios.newURI(node.url, null, null));
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


