# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#  Alec Flett <alecf@netscape.com>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****
// The history window uses JavaScript in bookmarksManager.js too.

var gHistoryTree;
var gLastHostname;
var gLastDomain;
var gGlobalHistory;
var gDeleteByHostname;
var gDeleteByDomain;
var gHistoryBundle;
var gHistoryStatus;
var gSearchBox;
var gHistoryGrouping = "";
var gWindowManager = null;

function HistoryCommonInit()
{
    gHistoryTree =  document.getElementById("historyTree");
    gDeleteByHostname = document.getElementById("menu_deleteByHostname");
    gDeleteByDomain =   document.getElementById("menu_deleteByDomain");
    gHistoryBundle =    document.getElementById("historyBundle");    
    gSearchBox = document.getElementById("search-box");

    var treeController = new nsTreeController(gHistoryTree);
    var mode = document.getElementById("viewButton").getAttribute("selectedsort");
    if (mode == "site")
      document.getElementById("bysite").setAttribute("checked", "true");
    else if (mode == "visited")
      document.getElementById("byvisited").setAttribute("checked", "true");
    else if (mode == "lastvisited")
      document.getElementById("bylastvisited").setAttribute("checked", "true");
    else if (mode == "dayandsite")
      document.getElementById("bydayandsite").setAttribute("checked", "true");
    else
      document.getElementById("byday").setAttribute("checked", "true");
    gHistoryTree.focus();
    gHistoryTree.treeBoxObject.view.selection.select(0);
}

function historyOnSelect()
{
    // every time selection changes, save the last hostname
    gLastHostname = "";
    gLastDomain = "";
    var match;
    var currentIndex = gHistoryTree.currentIndex;
    var rowIsContainer = (gHistoryGrouping == "day" || gHistoryGrouping == "dayandsite") ? isContainer(gHistoryTree, currentIndex) : false;
    var url = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder).getResourceAtIndex(currentIndex).Value;

    if (url && !rowIsContainer) {
        // matches scheme://(hostname)...
        match = url.match(/^.*?:\/\/(?:([^\/:]*)(?::([^\/:]*))?@)?([^\/:]*)(?::([^\/:]*))?(.*)$/);

        if (match && match.length>1)
            gLastHostname = match[3];
      
        if (gHistoryStatus)
            gHistoryStatus.label = url;
    }
    else {
        if (gHistoryStatus)
            gHistoryStatus.label = "";
    }

    if (gLastHostname) {
        // matches the last foo.bar in foo.bar or baz.foo.bar
        match = gLastHostname.match(/([^.]+\.[^.]+$)/);
        if (match)
            gLastDomain = match[1];
    }
    document.commandDispatcher.updateCommands("select");
}

function updateEditMenuitems() {
  var enabled = false, stringId;
  if (gLastHostname) {
    stringId = "deleteHost";
    enabled = true;
  } else {
    stringId = "deleteHostNoSelection";
  }
  var text = gHistoryBundle.getFormattedString(stringId, [ gLastHostname ]);
  gDeleteByHostname.setAttribute("label", text);
  gDeleteByHostname.setAttribute("disabled", enabled ? "false" : "true");

  enabled = false;
  if (gLastDomain) {
    stringId = "deleteDomain";
    enabled = true;
  } else {
    stringId = "deleteDomainNoSelection";
  }
  text = gHistoryBundle.getFormattedString(stringId, [ gLastDomain ]);
  gDeleteByDomain.setAttribute("label", text);
  gDeleteByDomain.setAttribute("disabled", enabled ? "false" : "true");
}

function deleteByHostname() {
  if (!gGlobalHistory)
      gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIBrowserHistory);
  gGlobalHistory.removePagesFromHost(gLastHostname, false)
  gHistoryTree.builder.rebuild();
}

function deleteByDomain() {
  if (!gGlobalHistory)
      gGlobalHistory = Components.classes["@mozilla.org/browser/global-history;1"].getService(Components.interfaces.nsIBrowserHistory);
  gGlobalHistory.removePagesFromHost(gLastDomain, true)
  gHistoryTree.builder.rebuild();
}

var historyDNDObserver = {
    onDragStart: function (aEvent, aXferData, aDragAction)
    {
        var currentIndex = gHistoryTree.currentIndex;
        if (isContainer(gHistoryTree, currentIndex))
            return false;
        var url = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "URL");
        var title = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "Name");

        var htmlString = "<A HREF='" + url + "'>" + title + "</A>";
        aXferData.data = new TransferData();
        aXferData.data.addDataForFlavour("text/unicode", url);
        aXferData.data.addDataForFlavour("text/html", htmlString);
        aXferData.data.addDataForFlavour("text/x-moz-url", url + "\n" + title);
        return true;
    }
};

function validClickConditions(event)
{
  var currentIndex = gHistoryTree.currentIndex;
  if (currentIndex == -1) return false;
  var container = isContainer(gHistoryTree, currentIndex);
  return (event.button == 0 &&
          event.originalTarget.localName == 'treechildren' &&
          !container);
}

function collapseExpand()
{
    var currentIndex = gHistoryTree.currentIndex;
    gHistoryTree.treeBoxObject.view.toggleOpenState(currentIndex);
}

function OpenURL(aInNewWindow)
{
    var currentIndex = gHistoryTree.currentIndex;     
    var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
    var url = builder.getResourceAtIndex(currentIndex).Value;
    
    if (aInNewWindow) {
      var count = gHistoryTree.treeBoxObject.view.selection.count;
      if (count == 1) {
        if (isContainer(gHistoryTree, currentIndex))
          openDialog("chrome://communicator/content/history/history.xul", 
                     "", "chrome,all,dialog=no", url, "newWindow");
        else      
          openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
      }
      else {
        var min = new Object(); 
        var max = new Object();
        var rangeCount = gHistoryTree.treeBoxObject.view.selection.getRangeCount();
        for (var i = 0; i < rangeCount; ++i) {
          gHistoryTree.treeBoxObject.view.selection.getRangeAt(i, min, max);
          for (var k = max.value; k >= min.value; --k) {
            url = gHistoryTree.treeBoxObject.view.getCellText(k, "URL");
            window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
          }
        }
      }
    }        
    else if (!isContainer(gHistoryTree, currentIndex))
      openTopWin(url);
    return true;
}

function SortBy(sortKey)
{
  // XXXBlake Welcome to the end of the world of Lame.
  // You can go no further. You are standing on the edge -- teetering, even.
  // Look on the bright side: you can rest assured that no code you see in the future
  // will even come close to the lameness of the code below.

  var sortDirection;
  switch(sortKey) {
    case "visited":
      sortKey = "VisitCount";    
      sortDirection = "ascending";
      break;
    case "name":
      sortKey = "Name";
      sortDirection = "natural";
      break;
    case "lastvisited":
      sortKey = "Date";
      sortDirection = "ascending";
      break;
    default:
      return;    
  }
  var col = document.getElementById(sortKey);
  col.setAttribute("sortDirection", sortDirection);
  gHistoryTree.treeBoxObject.view.cycleHeader(sortKey, col);
}

function GroupBy(groupingType)
{
    gHistoryGrouping = groupingType;
    switch(groupingType) {
    case "none":
        gHistoryTree.setAttribute("ref", "NC:HistoryRoot");
        break;
    case "site":
        // xxx for now
        gHistoryTree.setAttribute("ref", "NC:HistoryByDate");
        break;
    case "dayandsite":
        gHistoryTree.setAttribute("ref", "NC:HistoryByDateAndSite");
        break;
    case "day":
    default:
        gHistoryTree.setAttribute("ref", "NC:HistoryByDate");
        break;
    }
}

function historyAddBookmarks()
{
  var count = gHistoryTree.treeBoxObject.view.selection.count;
  var url;
  var title;
  if (count == 1) {
    var currentIndex = gHistoryTree.currentIndex;
    url = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "URL");
    title = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "Name");
    BookmarksUtils.addBookmark(url, title, undefined, true);
  }
  else if (count > 1) {
    var min = new Object(); 
    var max = new Object();
    var rangeCount = gHistoryTree.treeBoxObject.view.selection.getRangeCount();
    for (var i = 0; i < rangeCount; ++i) {
      gHistoryTree.treeBoxObject.view.selection.getRangeAt(i, min, max);
      for (var k = max.value; k >= min.value; --k) {
        url = gHistoryTree.treeBoxObject.view.getCellText(k, "URL");
        title = gHistoryTree.treeBoxObject.view.getCellText(k, "Name");
        BookmarksUtils.addBookmark(url, title, undefined, false);
      }
    }
  }
}


function updateItems()
{
  var count = gHistoryTree.treeBoxObject.view.selection.count;
  var openItem = document.getElementById("miOpen");
  var bookmarkItem = document.getElementById("miAddBookmark");
  var copyLocationItem = document.getElementById("miCopyLinkLocation");
  var sep1 = document.getElementById("pre-bookmarks-separator");
  var openItemInNewWindow = document.getElementById("miOpenInNewWindow");
  var collapseExpandItem = document.getElementById("miCollapseExpand");
  if (count > 1) {
    var hasContainer = false;
    if (gHistoryGrouping == "day") {
      var min = new Object(); 
      var max = new Object();
      var rangeCount = gHistoryTree.treeBoxObject.view.selection.getRangeCount();
      for (var i = 0; i < rangeCount; ++i) {
        gHistoryTree.treeBoxObject.view.selection.getRangeAt(i, min, max);
        for (var k = max.value; k >= min.value; --k) {
          if (isContainer(gHistoryTree, k)) {
            hasContainer = true;
            break;
          }
        }
      }
    }
    if (hasContainer) {
      bookmarkItem.setAttribute("hidden", "true");
      copyLocationItem.setAttribute("hidden", "true");
      sep1.setAttribute("hidden", "true");
      document.getElementById("post-bookmarks-separator").setAttribute("hidden", "true");
      openItem.setAttribute("hidden", "true");
      openItemInNewWindow.setAttribute("hidden", "true");
      collapseExpandItem.setAttribute("hidden", "true");
    }
    else {
      bookmarkItem.removeAttribute("hidden");
      copyLocationItem.removeAttribute("hidden");
      sep1.removeAttribute("hidden");
      bookmarkItem.setAttribute("label", document.getElementById('multipleBookmarks').getAttribute("label"));
      openItem.setAttribute("hidden", "true");
      openItem.removeAttribute("default");
      openItemInNewWindow.setAttribute("default", "true");
    }
  }
  else {
    bookmarkItem.setAttribute("label", document.getElementById('oneBookmark').getAttribute("label"));
    var currentIndex = gHistoryTree.currentIndex;
    if (isContainer(gHistoryTree, currentIndex)) {
        openItem.setAttribute("hidden", "true");
        openItem.removeAttribute("default");
        collapseExpandItem.removeAttribute("hidden");
        collapseExpandItem.setAttribute("default", "true");
        bookmarkItem.setAttribute("hidden", "true");
        copyLocationItem.setAttribute("hidden", "true");
        sep1.setAttribute("hidden", "true");
        if (isContainerOpen(gHistoryTree, currentIndex))
          collapseExpandItem.setAttribute("label", gHistoryBundle.getString("collapseLabel"));
        else
          collapseExpandItem.setAttribute("label", gHistoryBundle.getString("expandLabel"));
        return true;
    }
    collapseExpandItem.setAttribute("hidden", "true");
    bookmarkItem.removeAttribute("hidden");
    copyLocationItem.removeAttribute("hidden");
    sep1.removeAttribute("hidden");
    if (!gWindowManager) {
      gWindowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
      gWindowManager = gWindowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    }
    var topWindowOfType = gWindowManager.getMostRecentWindow("navigator:browser");
    if (!topWindowOfType) {
        openItem.setAttribute("hidden", "true");
        openItem.removeAttribute("default");
        openItemInNewWindow.setAttribute("default", "true");
    }
    else {
      openItem.removeAttribute("hidden");
      if (!openItem.getAttribute("default"))
        openItem.setAttribute("default", "true");
      openItemInNewWindow.removeAttribute("default");
    }
  }
  return true;
}

function searchHistory(aInput)
{
   if (!aInput) 
     GroupBy(gHistoryGrouping);
   else
     gHistoryTree.setAttribute("ref",
                               "find:datasource=history&match=Name&method=contains&text=" + escape(aInput));
 }
