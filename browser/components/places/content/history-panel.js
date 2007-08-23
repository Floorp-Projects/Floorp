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
#   Asaf Romano <mano@mozilla.com>
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

  initContextMenu();
  
  searchHistory("");

  gSearchBox.focus();
}

function initContextMenu() {
  // Force-hide items in the context menu which never apply to this view
  var alwaysHideElements = ["placesContext_new:bookmark",
                            "placesContext_new:folder",
                            "placesContext_new:separator",
                            "placesContext_cut",
                            "placesContext_paste",
                            "placesContext_sortBy:name"];
  for (var i=0; i < alwaysHideElements.length; i++) {
    var elt = document.getElementById(alwaysHideElements[i]);
    elt.removeAttribute("selection");
    elt.removeAttribute("forcehideselection");
    elt.hidden = true;
  }

  // Insert "Bookmark This Link" right before the copy item
  document.getElementById("placesContext")
          .insertBefore(document.getElementById("addBookmarkContextItem"),
                        document.getElementById("placesContext_copy"));
}

function GroupBy(groupingType)
{
  gHistoryGrouping = groupingType;
  gSearchBox.value = "";
  searchHistory("");
}

function historyAddBookmarks()
{ 
  // no need to check gHistoryTree.view.selection.count
  // node will be null if there is a multiple selection 
  // or if the selected item is not a URI node
  var node = gHistoryTree.selectedURINode;
  if (!node) 
    return;

  PlacesUtils.showMinimalAddBookmarkUI(PlacesUtils._uri(node.uri), node.title);
}

function searchHistory(aInput)
{
  var query = PlacesUtils.history.getNewQuery();
  var options = PlacesUtils.history.getNewQueryOptions();

  const NHQO = Ci.nsINavHistoryQueryOptions;
  var sortingMode;
  var groups = []; 
  var resultType;

  if (aInput) {
    query.searchTerms = aInput;
    sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
    resultType = NHQO.RESULTS_AS_URI;
  }
  else {
    switch (gHistoryGrouping) {
      case "visited":
        resultType = NHQO.RESULTS_AS_URI;
        sortingMode = NHQO.SORT_BY_VISITCOUNT_DESCENDING;
        break; 
      case "lastvisited":
        resultType = NHQO.RESULTS_AS_URI;
        sortingMode = NHQO.SORT_BY_DATE_DESCENDING;
        break; 
      case "dayandsite":  // fall through
        groups.push(NHQO.GROUP_BY_DAY);
      case "site":
        groups.push(NHQO.GROUP_BY_HOST);
        resultType = NHQO.RESULTS_AS_VISIT;
        sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
        break;
      default:
        resultType = NHQO.RESULTS_AS_VISIT;
        groups.push(NHQO.GROUP_BY_DAY);
        sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
        break;
    }
  }

  options.setGroupingMode(groups, groups.length);
  options.sortingMode = sortingMode;
  options.resultType = resultType;

  // call load() on the tree manually
  // instead of setting the place attribute in history-panel.xul
  // otherwise, we will end up calling load() twice
  gHistoryTree.load([query], options);
}
