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
    
    // XXXBlake we should persist the last search value
    // If it's empty, this will do the right thing and just group by the old grouping.
    searchHistory(gSearchBox.value);
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
        var url = builder.getResourceAtIndex(currentIndex).ValueUTF8;
        var col = gHistoryTree.columns["Name"];
        var title = gHistoryTree.view.getCellText(currentIndex, col);

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

function OpenURLIn(where)
{
  var count = gHistoryTree.view.selection.count;
  if (count != 1)
    return;

  var currentIndex = gHistoryTree.currentIndex;     
  if (isContainer(gHistoryTree, currentIndex))
    return;
 
  var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  var url = builder.getResourceAtIndex(currentIndex).ValueUTF8;
  
  if (!checkURLSecurity(url)) 
    return;

  openUILinkIn(url, where);
}

function OpenURL(event)
{
  OpenURLIn(whereToOpenLink(event));
}

function checkURLSecurity(aURL)
{
  var uri = Components.classes["@mozilla.org/network/standard-url;1"].
              createInstance(Components.interfaces.nsIURI);
  uri.spec = aURL;
  if (uri.schemeIs("javascript") || uri.schemeIs("data")) {
    var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                      .getService(Components.interfaces.nsIStringBundleService);
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    var historyBundle = strBundleService.createBundle("chrome://global/locale/history/history.properties");
    var brandBundle = strBundleService.createBundle("chrome://global/locale/brand.properties");      
    var brandStr = brandBundle.GetStringFromName("brandShortName");
    var errorStr = historyBundle.GetStringFromName("load-js-data-url-error");
    promptService.alert(window, brandStr, errorStr);
    return false;
  }
  return true;
}

function SortBy(sortKey)
{
  // We set the sortDirection to the one before we actually want it to be in the
  // cycle list, since cycleHeader cycles it forward before doing the sort.

  var sortDirection;
  switch(sortKey) {
    case "visited":
      sortKey = "rdf:http://home.netscape.com/NC-rdf#VisitCount";
      sortDirection = "ascending";
      break;
    case "name":
      sortKey = "rdf:http://home.netscape.com/NC-rdf#Name?sort=true";
      sortDirection = "natural";
      break;
    case "lastvisited":
      sortKey = "rdf:http://home.netscape.com/NC-rdf#Date";
      sortDirection = "ascending";
      break;
    default:
      return;    
  }
  var col = document.getElementById("Name");
  col.setAttribute("sort", sortKey);
  col.setAttribute("sortDirection", sortDirection);
  var column = gHistoryTree.columns.getColumnFor(col);
  gHistoryTree.view.cycleHeader(column);
}

function IsFindResource(uri)
{
  return (uri.substr(0, 5) == "find:");
}
    
function GroupBy(groupingType)
{
  var isFind = IsFindResource(groupingType);
  if (!isFind) {
    gHistoryGrouping = groupingType;
    gSearchBox.value = "";
  }
  switch(groupingType) {
    case "site":
      gHistoryTree.setAttribute("ref", "NC:HistoryRoot");
      break;
    case "dayandsite":
      gHistoryTree.setAttribute("ref", "NC:HistoryByDateAndSite");
      break;
    case "visited":
      gHistoryTree.setAttribute("ref", "NC:HistoryRoot");
      break;
    case "lastvisited":
      gHistoryTree.setAttribute("ref", "NC:HistoryRoot");
      break;
    case "day":
      gHistoryTree.setAttribute("ref", "NC:HistoryByDate");
      break;
    default:
      gHistoryTree.setAttribute("ref", groupingType);
  }
  Sort(isFind? gHistoryGrouping : groupingType);
}

function Sort(groupingType)
{
  switch(groupingType) {
    case "site":
    case "dayandsite":
    case "day":
      SortBy("name");
      break;
    case "lastvisited":
      SortBy("lastvisited");
      break;
    case "visited":
      SortBy("visited");
      break;
  }
}

function historyAddBookmarks()
{
  var count = gHistoryTree.view.selection.count;
  if (count != 1)
    return;
  
  var currentIndex = gHistoryTree.currentIndex;
  var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  var url = builder.getResourceAtIndex(currentIndex).ValueUTF8;
  
  //XXXBlake don't use getCellText
  var col = gHistoryTree.columns["Name"];
  var title = gHistoryTree.view.getCellText(currentIndex, col);
  BookmarksUtils.addBookmark(url, title, undefined);
}

function historyCopyLink()
{
  var builder = gHistoryTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
  var url = builder.getResourceAtIndex(gHistoryTree.currentIndex).ValueUTF8;
  var clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                            .getService(Components.interfaces.nsIClipboardHelper );
  clipboard.copyString(url);
}

function buildContextMenu()
{
  var count = gHistoryTree.view.selection.count;
  var openItem = document.getElementById("miOpen");
  var openItemInNewWindow = document.getElementById("miOpenInNewWindow");
  var openItemInNewTab = document.getElementById("miOpenInNewTab");
  var bookmarkItem = document.getElementById("miAddBookmark");
  var copyLocationItem = document.getElementById("miCopyLink");
  var sep1 = document.getElementById("pre-bookmarks-separator");
  var sep2 = document.getElementById("post-bookmarks-separator");
  var expandItem = document.getElementById("miExpand");
  var collapseItem = document.getElementById("miCollapse");
  if (count > 1) {
    openItem.hidden = true;
    openItemInNewWindow.hidden = true;
    openItemInNewTab.hidden = true;
    bookmarkItem.hidden = true;
    copyLocationItem.hidden = true;
    sep1.hidden = true;
    sep2.hidden = true;
    expandItem.hidden = true;    
    collapseItem.hidden = true;    
  }
  else {
    var currentIndex = gHistoryTree.currentIndex;
    if ((gHistoryGrouping == "day" || gHistoryGrouping == "dayandsite")
        && isContainer(gHistoryTree, currentIndex)) {
      openItem.hidden = true;
      openItemInNewWindow.hidden = true;
      openItemInNewTab.hidden = true;
      bookmarkItem.hidden = true;
      copyLocationItem.hidden = true;
      sep1.hidden = true;
      sep2.hidden = false;
      if (isContainerOpen(gHistoryTree, currentIndex)) {
        expandItem.hidden = true;
        collapseItem.hidden = false;
      } else {
        expandItem.hidden = false;
        collapseItem.hidden = true;
      }
    }
    else {
      openItem.hidden = false;
      openItemInNewWindow.hidden = false;
      openItemInNewTab.hidden = false;
      bookmarkItem.hidden = false;
      copyLocationItem.hidden = false;
      sep1.hidden = false;
      sep2.hidden = false;
      expandItem.hidden = true;
      collapseItem.hidden = true;
    }
  }
}

function searchHistory(aInput)
{
   if (aInput == "") {
     GroupBy(gHistoryGrouping);
     return;
   }
   
   GroupBy("find:datasource=history&match=Name&method=contains&text=" + encodeURIComponent(aInput));     
 }

