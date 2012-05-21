# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

function init() {
  document.getElementById("bookmarks-view").place =
    "place:queryType=1&folder=" + window.top.PlacesUIUtils.allBookmarksFolderId;
}

function searchBookmarks(aSearchString) {
  var tree = document.getElementById('bookmarks-view');
  if (!aSearchString)
    tree.place = tree.place;
  else
    tree.applyFilter(aSearchString,
                     [PlacesUtils.bookmarksMenuFolderId,
                      PlacesUtils.unfiledBookmarksFolderId,
                      PlacesUtils.toolbarFolderId]);
}

window.addEventListener("SidebarFocused",
                        function()
                          document.getElementById("search-box").focus(),
                        false);
