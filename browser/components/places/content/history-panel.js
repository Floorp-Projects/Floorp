# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
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
#   Alec Flett <alecf@netscape.com> (original author of history.js)
#   Seth Spitzer <sspizer@mozilla.org> (port to Places)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gHistoryTree;
var gSearchBox;
var gHistoryGrouping = "";
var gSearching = false;

function HistorySidebarInit()
{
  gHistoryTree = document.getElementById("historyTree");
  gSearchBox = document.getElementById("search-box");

  gHistoryGrouping = document.getElementById("viewButton").
                              getAttribute("selectedsort");

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
  // If it's empty, this will do the right thing and 
  // just group by the old grouping.
  // bug #359073 tracks this RFE
  searchHistory(gSearchBox.value);
  gSearchBox.focus();
}
  
function GroupBy(groupingType)
{
  gHistoryGrouping = groupingType;
  gSearchBox.value = "";
  searchHistory("");
}

function collapseExpand()
{
  var currentIndex = gHistoryTree.currentIndex; 
  gHistoryTree.view.toggleOpenState(currentIndex);
}

function historyAddBookmarks()
{ 
  // no need to check gHistoryTree.view.selection.count
  // node will be null if there is a multiple selection 
  // or if the selected item is not a URI node
  var node = gHistoryTree.selectedURINode;
  if (!node) 
    return;
  
#ifdef MOZ_PLACES_BOOKMARKS
  PlacesUtils.showAddBookmarkUI(PlacesUtils._uri(node.uri), node.title);
#else
  BookmarksUtils.addBookmark(node.uri, node.title, undefined);
#endif
}

function historyCopyLink()
{
  var node = gHistoryTree.selectedURINode;
  var clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                            .getService(Ci.nsIClipboardHelper);
  clipboard.copyString(node.uri);
}   

function buildContextMenu(aEvent)
{
  var view = gHistoryTree.view;
  // if nothing is selected, bail and don't show a context menu
  if (view.selection.count != 1) {
    aEvent.preventDefault();
    return;
  }

  var openItem = document.getElementById("miOpen");
  var openItemInNewWindow = document.getElementById("miOpenInNewWindow");
  var openItemInNewTab = document.getElementById("miOpenInNewTab");
  var bookmarkItem = document.getElementById("miAddBookmark");
  var copyLocationItem = document.getElementById("miCopyLink");
  var sep1 = document.getElementById("pre-bookmarks-separator");
  var sep2 = document.getElementById("post-bookmarks-separator");
  var expandItem = document.getElementById("miExpand");
  var collapseItem = document.getElementById("miCollapse");

  var currentIndex = gHistoryTree.currentIndex;
  if ((gHistoryGrouping == "day" || gHistoryGrouping == "dayandsite")
      && view.isContainer(currentIndex)) {
    openItem.hidden = true;
    openItemInNewWindow.hidden = true;
    openItemInNewTab.hidden = true;
    bookmarkItem.hidden = true;
    copyLocationItem.hidden = true;
    sep1.hidden = true;
    sep2.hidden = false;

    if (view.isContainerOpen(currentIndex)) {
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

function SetSortingAndGrouping() {
  const NHQO = Ci.nsINavHistoryQueryOptions;
  var sortingMode;
  var groups = [];
  switch (gHistoryGrouping) {
    case "site":
      sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
      break; 
    case "visited":
      sortingMode = NHQO.SORT_BY_VISITCOUNT_DESCENDING;
      break; 
    case "lastvisited":
      sortingMode = NHQO.SORT_BY_DATE_DESCENDING;
      break; 
    case "dayandsite":
      groups.push(NHQO.GROUP_BY_DAY);
      groups.push(NHQO.GROUP_BY_HOST);
      sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
      break;
    default:
      groups.push(NHQO.GROUP_BY_DAY);
      sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
      break;
  }
  var options = asQuery(gHistoryTree.getResult().root).queryOptions;
  options.setGroupingMode(groups, groups.length);
  options.sortingMode = sortingMode;
}

function searchHistory(aInput)
{
  if (aInput) {
    if (!gSearching) {
      // Unset grouping when searching; applyFilter will update the view
      var options = asQuery(gHistoryTree.getResult().root).queryOptions;
      options.setGroupingMode([], 0);
      gSearching = true;
    }
  }
  else {
    // applyFilter will update the view
    SetSortingAndGrouping();
    gSearching = false;
  }

  gHistoryTree.applyFilter(aInput, false /* onlyBookmarks */, 
                           0 /* folderRestrict */, null); 
}

#include ../../../../toolkit/content/debug.js
