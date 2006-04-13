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
 * The Original Code is Mozilla Places Popup.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Annie Sullivan <annie.sullivan@gmail.com>
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


/**
 * Sets the place URI in the tree and menulist. 
 * This function is global so it can be easily accessed by openers. 
 * @param   placeURI
 *          A place: URI string to select
 */
function setPlaceURI(placeURI) {
  // Set the place for the tree.
  PlacesPopup._tree.place = placeURI;
  
  // Set the selected item in the menulist.
  for (var i = 0; i < PlacesPopup._popup.childNodes.length; i++) {
    if (PlacesPopup._popup.childNodes[i].getAttribute("url") == placeURI) {
      PlacesPopup._menulist.selectedIndex = i;
      break;
    }
  }
}

/**
 * Functions for handling events related to the places button and its popup.
 */
var PlacesPopup = {

  /**
   * UI Text Strings
   */
  __strings: null,
  get _strings() {
    if (!this.__strings)
      this.__strings = document.getElementById("stringBundle");
    return this.__strings;
  },
  
  /**
   * Places tree
   */
  __tree: null,
  get _tree() {
    if (!this.__tree)
      this.__tree = document.getElementById("placesPopupTree");
    return this.__tree;
  },
  
  /**
   * Search results tree
   */
  __searchTree: null,
  get _searchTree() {
    if (!this.__searchTree)
      this.__searchTree = document.getElementById("placesPopupSearchTree");
    return this.__searchTree;
  },
  
  /**
   * Places menulist popup
   */
  __popup: null,
  get _popup() {
    if (!this.__popup)
      this.__popup = document.getElementById("placesPopupViewMenuPopup");
    return this.__popup;
  },
  
  /**
   * Places menulist
   */
  __menulist: null,
  get _menulist() {
    if (!this.__menulist)
      this.__menulist = document.getElementById("placesPopupView");
    return this.__menulist;
  },

  /**
   * Result used to populate drop-down menu.
   */
  _menuResult: null,

  /**
   * Initializes the popup
   */
  init: function() {    
    // Select the correct place to view in the menulist and tree.
    this._popup._rebuild();
    setPlaceURI(window.arguments[0]);
  },
  
  /**
   * Called when the text in the search textbox changes.
   * Show or hide the search results depending on whether text was typed.
   * If text was typed, run a query to get new search results.
   * @param event
   *        DOMEvent for the textbox command.
   */
  onSearchTextChanged: function PP_onSearchTextChanged(event) {
    var searchbox = document.getElementById("placesPopupSearch");
    var searchResults = document.getElementById("placesPopupSearchResults");
    if (searchbox.value == "") {
      // Empty searchbox, hide search results
      searchResults.hidden = true;
    }
    else {
      // New text in the searchbox, show search results
      searchResults.hidden = false;
      
      // Tree view shouldn't show at the same time as search results.
      var expandButton = document.getElementById("placesPopupExpand");
      this.expandTreeView(false, expandButton);
      
      // Set filter string to search.
      this._searchTree.filterString = searchbox.value;
    }
    
    // Update the window size.
    window.sizeToContent();
  },
  
  /**
   * Called when the menuitem in the place menulist is selected.
   */
  onPlaceMenuitemSelected: function PP_onPlaceMenuitemSelected(event) {
    this._tree.place = event.target.getAttribute("url");
  },
  
  /**
   * Called when the expand button is pressed.  Toggles whether the
   * places tree, menulist, and organize button are showing.
   */
  onExpandButtonCommand: function PP_onExpandButtonCommand(event) {
    var expandButton = document.getElementById("placesPopupExpand");
    this.expandTreeView(expandButton.getAttribute("expanded") == "false", expandButton);
    
    // Update the window size.
    window.sizeToContent();
  },
  
  /**
   * Utility function to expand or collapse tree view
   * @param expand
   *        boolean, expand if true or collapse if false
   * @param expandButton
   *        XULElement for the expand/collapse button
   */
  expandTreeView: function PP_expandTreeView(expand, expandButton) {
    var treeContainer = document.getElementById("placesPopupTreeContainer");
    var organizeButton = document.getElementById("placesPopupOrganize");
    if (expand) {
      // Expand the tree view.
      treeContainer.hidden = false;
      organizeButton.hidden = false;
      expandButton.setAttribute("label", this._strings.getString("collapseButtonLabel"));
      expandButton.setAttribute("expanded", "true");
      
      // Search results should be collapsed while tree view is visible.
      var searchResults = document.getElementById("placesPopupSearchResults");
      searchResults.hidden = true;
    }
    else {
      // Collapse the tree view.
      treeContainer.hidden = true;
      organizeButton.hidden = true;
      expandButton.setAttribute("label", this._strings.getString("expandButtonLabel"));
      expandButton.setAttribute("expanded", "false");
    }
  },
  
  /**
   * Handles double-clicking of a link in the places tree.
   * @param event
   *        DOMEvent for the double click.
   */
  onLinkDoubleClicked: function PP_onLinkDoubleClicked(event) {
    // Load the link and hide the places window.
    PlacesController.mouseLoadURI(event);
    window.opener.PlacesCommandHook.hidePlacesPopup();
  }
};

#include ../../../../toolkit/content/debug.js
