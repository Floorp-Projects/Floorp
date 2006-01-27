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
 * The Original Code is Mozilla History System
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

var PlacesBrowserShim = {
  _bms: null,
  _lms: null,
  _history: null,
};

PlacesBrowserShim.init = function PBS_init() {
  var addBookmarkCmd = document.getElementById("Browser:AddBookmarkAs");
  addBookmarkCmd.setAttribute("oncommand", "PlacesBrowserShim.addBookmark()");

  this._bms = 
      Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService);

  this._lms = 
      Cc["@mozilla.org/browser/livemark-service;1"].
      getService(Ci.nsILivemarkService);
      
  this._hist = 
      Cc["@mozilla.org/browser/nav-history-service;1"].
      getService(Ci.nsINavHistoryService);

  // Override the old addLivemark function
  BookmarksUtils.addLivemark = function(a,b,c,d) {PlacesBrowserShim.addLivemark(a,b,c,d);};
  
  var newMenuPopup = document.getElementById("bookmarksMenuPopup");
  var query = this._hist.getNewQuery();
  query.setFolders([this._bms.bookmarksRoot], 1);
  var options = this._hist.getNewQueryOptions();
  options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
  options.expandQueries = true;
  var result = this._hist.executeQuery(query, options);
  newMenuPopup._result = result;
  newMenuPopup._resultNode = result.root;
};

PlacesBrowserShim.addBookmark = function PBS_addBookmark() {
  var selectedBrowser = getBrowser().selectedBrowser;
  this._bookmarkURI(this._bms.bookmarksRoot, selectedBrowser.currentURI, 
                    selectedBrowser.contentTitle);
};

PlacesBrowserShim._bookmarkURI = 
function PBS__bookmarkURI(folder, uri, title) {
  this._bms.insertItem(folder, uri, -1);
  this._bms.setItemTitle(uri, title);
};

PlacesBrowserShim.showHistory = function PBS_showHistory() {
  loadURI("chrome://browser/content/places/places.xul?history", null, null);
};

PlacesBrowserShim.showBookmarks = function PBS_showBookmarks() {
  loadURI("chrome://browser/content/places/places.xul?bookmarks", null, null);
};

PlacesBrowserShim.addLivemark = function PBS_addLivemark(aURL, aFeedURL, aTitle, aDescription) {
  // TODO -- put in nice confirmation dialog.
  var ios = 
      Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);

  this._lms.createLivemark(this._bms.toolbarRoot,
                          aTitle,
                          ios.newURI(aURL, null, null),
                          ios.newURI(aFeedURL, null, null),
                          -1);
};

addEventListener("load", function () { PlacesBrowserShim.init(); }, false);
