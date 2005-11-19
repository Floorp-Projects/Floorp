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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

var PlacesBrowserShim = {
  _bookmarks: null,
  _history: null,
};

PlacesBrowserShim.init = function PBS_init() {
  var addBookmarkCmd = document.getElementById("Browser:AddBookmarkAs");
  var addBookmarksCmd = document.getElementById("Browser:BookmarkAllTabs");
  var addBookmarksItem = document.getElementById("bookmarkAllCmd");
  var viewBookmarksItem = addBookmarksItem.nextSibling;
  var endHistoryItem = document.getElementById("endHistorySeparator");
  var viewHistoryItem = endHistoryItem.nextSibling;
  
  addBookmarkCmd.setAttribute("oncommand", "PlacesBrowserShim.addBookmark()");
  addBookmarksCmd.setAttribute("oncommand", "PlacesBrowserShim.addBookmarks()");
  viewBookmarksItem.setAttribute("oncommand", "PlacesBrowserShim.showPlaces()");
  //viewHistoryItem.setAttribute("oncommand", "PlacesBrowserShim.showPlaces");

  PlacesBrowserShim._bookmarks = 
      Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService);
};

PlacesBrowserShim.addBookmark = function PBS_addBookmark() {
  var selectedBrowser = getBrowser().selectedBrowser;
  this._bookmarkURI(this._bookmarks.bookmarkRoot, selectedBrowser.currentURI, 
                    selectedBrowser.contentTitle);
};

PlacesBrowserShim.addBookmarks = function PBS_addBookmarks() {
  var folder = this._bookmarks.createFolder(this._bookmarks.bookmarksRoot, 
                                            "Goats", -1);
  var browsers = getBrowser().browsers;
  for (var i = 0; i < browsers.length; ++i)
    this._bookmarkURI(folder, browsers[i].currentURI, 
                      browsers[i].contentTitle);
};

PlacesBrowserShim._bookmarkURI = 
function PBS__bookmarkURI(folder, uri, title) {
  this._bookmarks.insertItem(folder, uri, -1);
  this._bookmarks.setItemTitle(uri, title);
};

PlacesBrowserShim.showPlaces = function PBS_showPlaces() {
  loadURI("chrome://browser/content/places/places.xul", null, null);
};

addEventListener("load", PlacesBrowserShim.init, false);
