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
var gGlobalHistory;
var gSearchBox;
var gHistoryGrouping = "";

function HistoryCommonInit()
{
    gHistoryTree =  document.getElementById("historyTree");    
    gSearchBox = document.getElementById("search-box");

    var treeController = new nsTreeController(gHistoryTree);
    gHistoryGrouping = document.getElementById("viewButton").getAttribute("selectedsort");
    if (gHistoryGrouping == "site")
      document.getElementById("bysite").setAttribute("checked", "true");
    else if (gHistoryGrouping == "visited")
      document.getElementById("byvisited").setAttribute("checked", "true");
    else if (gHistoryGrouping == "lastvisited")
      document.getElementById("bylastvisited").setAttribute("checked", "true");
    else if (gHistoryGrouping == "dayandsite")
      document.getElementById("bydayandsite").setAttribute("checked", "true");
    else
      document.getElementById("byday").setAttribute("checked", "true");
    gHistoryTree.focus();
    gHistoryTree.treeBoxObject.view.selection.select(0);
}

function historyOnSelect()
{
  document.commandDispatcher.updateCommands("select");
}

var historyDNDObserver = {
    onDragStart: function (aEvent, aXferData, aDragAction)
    {
        var currentIndex = gHistoryTree.currentIndex;
        if (isContainer(gHistoryTree, currentIndex))
            return false;
        var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
        var url = builder.getResourceAtIndex(currentIndex).Value;
        var title = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "Name");

        var htmlString = "<A HREF='" + url + "'>" + title + "</A>";
        aXferData.data = new TransferData();
        aXferData.data.addDataForFlavour("text/unicode", url);
        aXferData.data.addDataForFlavour("text/html", htmlString);
        aXferData.data.addDataForFlavour("text/x-moz-url", url + "\n" + title);
        return true;
    }
};

function collapseExpand()
{
    var currentIndex = gHistoryTree.currentIndex;
    gHistoryTree.treeBoxObject.view.toggleOpenState(currentIndex);
}

function onDoubleClick(event)
{
  if (event.metaKey || event.shiftKey)
    OpenURL(1);
  else if (event.ctrlKey)
    OpenURL(2, event);
  else
    OpenURL(0);
}

function OpenURL(aWhere, event)
{
  var count = gHistoryTree.treeBoxObject.view.selection.count;
  if (count != 1)
    return;

  var currentIndex = gHistoryTree.currentIndex;     
  if (isContainer(gHistoryTree, currentIndex))
    return;
 
  var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  var url = builder.getResourceAtIndex(currentIndex).Value;

  if (aWhere == 0)
    openTopWin(url);
  else if (aWhere == 1)
    openNewWindowWith(url, null, false);
  else
    openNewTabWith(url, null, event, false);     
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
    gSearchBox.value = "";
}

function historyAddBookmarks()
{
  var count = gHistoryTree.treeBoxObject.view.selection.count;
  if (count != 1)
    return;
  
  var currentIndex = gHistoryTree.currentIndex;
  var url = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "URL");
  var title = gHistoryTree.treeBoxObject.view.getCellText(currentIndex, "Name");
  BookmarksUtils.addBookmark(url, title, undefined, true);
}


function buildContextMenu()
{
  var count = gHistoryTree.treeBoxObject.view.selection.count;
  var openItem = document.getElementById("miOpen");
  var openItemInNewWindow = document.getElementById("miOpenInNewWindow");
  var openItemInNewTab = document.getElementById("miOpenInNewTab");
  var bookmarkItem = document.getElementById("miAddBookmark");
  var copyLocationItem = document.getElementById("miCopyLinkLocation");
  var sep1 = document.getElementById("pre-bookmarks-separator");
  var sep2 = document.getElementById("post-bookmarks-separator");
  var collapseExpandItem = document.getElementById("miCollapseExpand");
  if (count > 1) {
    openItem.hidden = true;
    openItemInNewWindow.hidden = true;
    openItemInNewTab.hidden = true;
    bookmarkItem.hidden = true;
    copyLocationItem.hidden = true;
    sep1.hidden = true;
    sep2.hidden = true;
    collapseExpandItem.hidden = true;    
  }
  else {
    var currentIndex = gHistoryTree.currentIndex;
    if ((gHistoryGrouping == "day" || gHistoryGrouping == "dayandsite")
        && isContainer(gHistoryTree, currentIndex)) {
      var bundle = document.getElementById("historyBundle");    
      openItem.hidden = true;
      openItemInNewWindow.hidden = true;
      openItemInNewTab.hidden = true;
      bookmarkItem.hidden = true;
      copyLocationItem.hidden = true;
      sep1.hidden = true;
      sep2.hidden = false;
      collapseExpandItem.hidden = false;      
      if (isContainerOpen(gHistoryTree, currentIndex))
        collapseExpandItem.setAttribute("label", bundle.getString("collapseLabel"));
      else
        collapseExpandItem.setAttribute("label", bundle.getString("expandLabel"));
    }
    else {
      openItem.hidden = false;
      openItemInNewWindow.hidden = false;
      openItemInNewTab.hidden = false;
      bookmarkItem.hidden = false;
      copyLocationItem.hidden = false;
      sep1.hidden = false;
      sep2.hidden = false;
      collapseExpandItem.hidden = true;
    }
  }
}

function searchHistory(aInput)
{
   if (!aInput) 
     GroupBy(gHistoryGrouping);
   else
     gHistoryTree.setAttribute("ref",
                               "find:datasource=history&match=Name&method=contains&text=" + escape(aInput));
 }
