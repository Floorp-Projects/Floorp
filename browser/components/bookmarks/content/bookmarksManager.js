# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
# 
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@netscape.com> (Original Author, v3.0)
#   Pierre Chanial <chanial@noos.fr>

var gSearchBox;

////////////////////////////////////////////////////////////////////////////////
// Initialize the command controllers, set focus, tree root, 
// window title state, etc. 
function Startup()
{
  const windowNode = document.getElementById("bookmark-window");
  const bookmarksView = document.getElementById("bookmarks-view");
  const bookmarksFolder = document.getElementById("bookmark-folders-view");

  var titleString;

  // If we've been opened with a parameter, root the tree on it.
  if ("arguments" in window && window.arguments[0]) {
    var title;
    var uri = window.arguments[0];
    bookmarksView.tree.setAttribute("ref", uri);
    bookmarksFolder.setAttribute("hidden","true")
    document.getElementById("bookmarks-search").setAttribute("hidden","true")
    document.getElementById("bookmark-views-splitter").setAttribute("hidden","true")    
    if (uri.substring(0,5) == "find:") {
      title = BookmarksUtils.getLocaleString("search_results_title");
      // Update the windowtype so that future searches are directed 
      // there and the window is not re-used for bookmarks. 
      windowNode.setAttribute("windowtype", "bookmarks:searchresults");
    }
    else
      title = BookmarksUtils.getProperty(window.arguments[0], NC_NS+"Name");
    
    titleString = BookmarksUtils.getLocaleString("window_title", title);
  }
  else {
    titleString = BookmarksUtils.getLocaleString("bookmarks_title", title);
    // always open the bookmark root folder
    if (!bookmarksFolder.treeBoxObject.view.isContainerOpen(0))
      bookmarksFolder.treeBoxObject.view.toggleOpenState(0);
    bookmarksFolder.treeBoxObject.view.selection.select(0);
  }

  bookmarksView.treeBoxObject.view.selection.select(0);

  windowNode.setAttribute("title", titleString);

  document.getElementById("CommandUpdate_Bookmarks").setAttribute("commandupdater","true");
  bookmarksView.focus();

  BMSVC.transactionManager.AddListener(BookmarkEditMenuTxnListener);

}

function Shutdown()
{
  BMSVC.transactionManager.RemoveListener(BookmarkEditMenuTxnListener);
  // Store current window position and size in window attributes (for persistence).
  var win = document.getElementById("bookmark-window");
  win.setAttribute("x", screenX);
  win.setAttribute("y", screenY);
  win.setAttribute("height", outerHeight);
  win.setAttribute("width", outerWidth);
}

var gConstructedViewMenuSortItems = false;
function fillViewMenu(aEvent)
{
  var adjacentElement = document.getElementById("fill-before-this-node");
  var popupElement = aEvent.target;
  
  var bookmarksView = document.getElementById("bookmarks-view");
  var columns = bookmarksView.columns;

  if (!gConstructedViewMenuSortItems) {
    for (var i = 0; i < columns.length; ++i) {
      var accesskey = columns[i].accesskey;
      var menuitem  = document.createElement("menuitem");
      var name      = BookmarksUtils.getLocaleString("SortMenuItem", columns[i].label);
      menuitem.setAttribute("label", name);
      menuitem.setAttribute("accesskey", columns[i].accesskey);
      menuitem.setAttribute("resource", columns[i].resource);
      menuitem.setAttribute("id", "sortMenuItem:" + columns[i].resource);
      menuitem.setAttribute("checked", columns[i].sortActive);
      menuitem.setAttribute("name", "sortSet");
      menuitem.setAttribute("type", "radio");
      
      popupElement.insertBefore(menuitem, adjacentElement);
    }
    
    gConstructedViewMenuSortItems = true;
  }  

  const kPrefSvcContractID = "@mozilla.org/preferences-service;1";
  const kPrefSvcIID = Components.interfaces.nsIPrefService;
  var prefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
  var bookmarksSortPrefs = prefSvc.getBranch("browser.bookmarks.sort.");

  if (gConstructedViewMenuSortItems) {
    var resource = bookmarksSortPrefs.getCharPref("resource");
    var element = document.getElementById("sortMenuItem:" + resource);
    if (element)
      element.setAttribute("checked", "true");
  }  

  var sortAscendingMenu = document.getElementById("ascending");
  var sortDescendingMenu = document.getElementById("descending");
  var noSortMenu = document.getElementById("natural");
  
  sortAscendingMenu.setAttribute("checked", "false");
  sortDescendingMenu.setAttribute("checked", "false");
  noSortMenu.setAttribute("checked", "false");
  var direction = bookmarksSortPrefs.getCharPref("direction");
  if (direction == "natural")
    sortAscendingMenu.setAttribute("checked", "true");
  else if (direction == "ascending") 
    sortDescendingMenu.setAttribute("checked", "true");
  else
    noSortMenu.setAttribute("checked", "true");
}

function onViewMenuSortItemSelected(aEvent)
{
  var resource = aEvent.target.getAttribute("resource");
  
  const kPrefSvcContractID = "@mozilla.org/preferences-service;1";
  const kPrefSvcIID = Components.interfaces.nsIPrefService;
  var prefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
  var bookmarksSortPrefs = prefSvc.getBranch("browser.bookmarks.sort.");

  switch (resource) {
  case "":
    break;
  case "direction":
    var dirn = bookmarksSortPrefs.getCharPref("direction");
    if (aEvent.target.id == "ascending")
      bookmarksSortPrefs.setCharPref("direction", "natural");
    else if (aEvent.target.id == "descending")
      bookmarksSortPrefs.setCharPref("direction", "ascending");
    else
      bookmarksSortPrefs.setCharPref("direction", "descending");
    break;
  default:
    bookmarksSortPrefs.setCharPref("resource", resource);
    var direction = bookmarksSortPrefs.getCharPref("direction");
    if (direction == "descending")
      bookmarksSortPrefs.setCharPref("direction", "natural");
    break;
  }

  aEvent.preventCapture();
}  

var gConstructedColumnsMenuItems = false;
function fillColumnsMenu(aEvent) 
{
  var bookmarksView = document.getElementById("bookmarks-view");
  var columns = bookmarksView.columns;
  var i;

  if (!gConstructedColumnsMenuItems) {
    for (i = 0; i < columns.length; ++i) {
      var menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", columns[i].label);
      menuitem.setAttribute("resource", columns[i].resource);
      menuitem.setAttribute("id", "columnMenuItem:" + columns[i].resource);
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("checked", columns[i].hidden != "true");
      aEvent.target.appendChild(menuitem);
    }

    gConstructedColumnsMenuItems = true;
  }
  else {
    for (i = 0; i < columns.length; ++i) {
      var element = document.getElementById("columnMenuItem:" + columns[i].resource);
      if (element && columns[i].hidden != "true")
        element.setAttribute("checked", "true");
    }
  }
  
  aEvent.preventBubble();
}

function onViewMenuColumnItemSelected(aEvent)
{
  var resource = aEvent.target.getAttribute("resource");
  if (resource != "") {
    var bookmarksView = document.getElementById("bookmarks-view");
    bookmarksView.toggleColumnVisibility(resource);
  }  

  aEvent.preventBubble();
}

function onViewSelected(aEvent)
{
  var statusBar = document.getElementById("statusbar-text");
  var displayValue;
  var selection = aEvent.target.getTreeSelection();
  var bookmarksView = document.getElementById("bookmarks-view");
  
  if (aEvent.target.id == "bookmark-folders-view" && selection)
    bookmarksView.tree.setAttribute("ref",selection.item[0].Value);
      
  if (statusBar && selection.length == 1) {
    //protocol broken since we have unique ID...
    //var protocol = selection.protocol[0];
    if (selection.isContainer[0]) {// && protocol != "find" && protocol != "file") {
      RDFC.Init(aEvent.target.db, selection.item[0]);
      var count = RDFC.GetCount();
      displayValue = BookmarksUtils.getLocaleString("status_foldercount", String(count));
    }
    else if (selection.type[0] == "Bookmark")
      displayValue = BookmarksUtils.getProperty(selection.item[0], NC_NS+"URL", aEvent.target.db)
    else
      displayValue = "";
    statusBar.label = displayValue;
  }
}

