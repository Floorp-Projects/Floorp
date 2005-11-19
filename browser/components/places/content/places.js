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
 * The Original Code is Mozilla Places System.
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

var PlacesPage = { 
  _controller: null,
  _tabbrowser: null,
  _topWindow: null,
  _topDocument: null,
  _populators: { },
  _bmsvc : null,
};

PlacesPage.init = function PP_init() {
  var wm =
      Cc["@mozilla.org/appshell/window-mediator;1"].
      getService(Ci.nsIWindowMediator);
  this._topWindow = wm.getMostRecentWindow("navigator:browser");
  this._topDocument = this._topWindow.document;
  this._tabbrowser = this._topWindow.getBrowser();

  var self = this;
  function onTabSelect(event) {
    self.onTabSelect(event);
  }
  this._tabbrowser.mTabContainer.addEventListener("select", onTabSelect, false);

  var placesList = document.getElementById("placesList");
  var placeContent = document.getElementById("placeContent");
  var placesCommands = document.getElementById("placesCommands");
  
  this.controller = 
    new PlacesController([placesList, placeContent], placesCommands);

  const NH = Ci.nsINavHistory;  
  const NHQO = Ci.nsINavHistoryQueryOptions;
  const TV = Ci.nsITreeView;
  var places = 
      Cc["@mozilla.org/browser/nav-history;1"].getService(NH);
  var query = places.getNewQuery();
  var date = new Date();
  var options = places.getNewQueryOptions();
  options.setGroupingMode([NHQO.GROUP_BY_HOST], 1);
  options.setSortingMode(NHQO.SORT_BY_NONE);
  options.setResultType(NHQO.RESULT_TYPE_URL);
  var result = places.executeQuery(query, options);
  result.QueryInterface(TV);
  var placeContent = document.getElementById("placeContent");
  placeContent.view = result;

  const BS = Ci.nsINavBookmarksService;
  this._bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(BS);
  var children = this._bmsvc.getFolderChildren(this._bmsvc.placesRoot,
                                               BS.FOLDER_CHILDREN);
  document.getElementById("placesList").view = children.QueryInterface(TV);

  this._showPlacesUI();
};

PlacesPage.uninit = function PP_uninit() {
  this._hidePlacesUI();
};

PlacesPage.onTabSelect = function PP_onTabSelect(event) {
  var tabURI = this._tabbrowser.selectedBrowser.currentURI.spec;
  (tabURI == PLACES_URI) ? this._showPlacesUI() : this._hidePlacesUI();
};

PlacesPage._showPlacesUI = function PP__showPlacesUI() {
  LOG("SHOW Places UI");
  this._tabbrowser.setAttribute("places", "true");
  var statusbar = this._topDocument.getElementById("status-bar");
  this._oldStatusBarState = statusbar.hidden;
  statusbar.hidden = true;
};

PlacesPage._hidePlacesUI = function PP__hidePlacesUI() {
  LOG("HIDE Places UI");
  this._tabbrowser.removeAttribute("places");
  var statusbar = this._topDocument.getElementById("status-bar");
  statusbar.hidden = this._oldStatusBarState;
};

PlacesPage.loadQuery = function PP_loadQuery(uri) {
  var placeURI = uri.QueryInterface(Ci.mozIPlaceURI);
  this._updateUI(placeURI.providerType);
};

PlacesPage.showAdvancedOptions = function PP_showAdvancedOptions() {
  alert("Show advanced query builder.");
};

PlacesPage.setFilterCollection = function PP_setFilterCollection(collectionName) {
  var searchFilter = document.getElementById("searchFilter");
  searchFilter.setAttribute("collection", collectionName);
};

PlacesPage.applyFilter = function PP_applyFilter(filterString) {
  var searchFilter = document.getElementById("searchFilter");
  var collectionName = searchFilter.getAttribute("collection");
  if (collectionName == "collection") {
    alert("Search Only This Collection Not Yet Supported");
    this.setFilterCollection("all");
  }
  else if (collectionName == "all") {
    this._buildQuery(filterString);
  }
};

PlacesPage._buildQuery = function PP__buildQuery(filterString) {
  const NH = Ci.nsINavHistory;  
  const NHQO = Ci.nsINavHistoryQueryOptions;
  var places = Cc["@mozilla.org/browser/nav-history;1"].getService(NH);
  var query = places.getNewQuery();
  query.searchTerms = filterString;
  var options = places.getNewQueryOptions();
  options.setGroupingMode([NHQO.GROUP_BY_HOST], 1);
  options.setSortingMode(NHQO.SORT_BY_NONE);
  options.setResultType(NHQO.RESULT_TYPE_URL);
  var result = places.executeQuery(query, options);
  result.QueryInterface(Ci.nsITreeView);
  var placeContent = document.getElementById("placeContent");
  placeContent.view = result;
  placeContent.query = query;
  placeContent.options = options;
};

PlacesPage._getLoadFunctionForEvent = 
function PP__getLoadFunctionForEvent(event) {
  if (event.button != 0)
    return null;
  
  if (event.ctrlKey)
    return this.openLinkInNewTab;
  else if (event.shiftKey)
    return this.openLinkInNewWindow;
  return this.openLinkInCurrentWindow;
};

/**
 * Loads a URL in the appropriate tab or window, given the user's preference
 * specified by modifier keys tracked by a DOM event
 * @param   event
 *          The DOM Mouse event with modifier keys set that track the user's
 *          preferred destination window or tab.
 */
PlacesPage.mouseLoadURIInBrowser = function PP_loadURIInBrowser(event) {
  this._getLoadFunctionForEvent(event)();
};

/**
 * Loads the selected URL in a new tab. 
 */
PlacesPage.openLinkInNewTab = function PP_openLinkInNewTab() {
  var placeContent = document.getElementById("placeContent");
  this._topWindow.openNewTabWith(placeContent.selectedURL, null, null);
};

/**
 * Loads the selected URL in a new window.
 */
PlacesPage.openLinkInNewWindow = function PP_openLinkInNewWindow() {
  var placeContent = document.getElementById("placeContent");
  this._topWindow.openNewWindowWith(placeContent.selectedURL, null, null);
};

/**
 * Loads the selected URL in the current window, replacing the Places page.
 */
PlacesPage.openLinkInCurrentWindow = function PP_openLinkInCurrentWindow() {
  var placeContent = document.getElementById("placeContent");
  this._topWindow.loadURI(placeContent.selectedURL, null, null);
};

/**
 * Called when a place folder is selected in the left pane.
 */
PlacesPage.placeSelected = function PP_placeSelected(event) {
  var resultView = event.target.view;
  resultView.QueryInterface(Components.interfaces.nsINavHistoryResult);

  var folder = resultView.nodeForTreeIndex(resultView.selection.currentIndex);
  document.getElementById("placeContent").view = this._bmsvc.getFolderChildren(folder.folderId, Components.interfaces.nsINavBookmarksService.ALL_CHILDREN);
};
