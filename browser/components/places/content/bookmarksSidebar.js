/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Shared Places Import - change other consumers if you change this: */
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  PlacesTransactions: "resource://gre/modules/PlacesTransactions.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyScriptGetter(
  this,
  "PlacesTreeView",
  "chrome://browser/content/places/treeView.js"
);
XPCOMUtils.defineLazyScriptGetter(
  this,
  ["PlacesInsertionPoint", "PlacesController", "PlacesControllerDragHelper"],
  "chrome://browser/content/places/controller.js"
);
/* End Shared Places Import */
var gCumulativeSearches = 0;

function init() {
  let uidensity = window.top.document.documentElement.getAttribute("uidensity");
  if (uidensity) {
    document.documentElement.setAttribute("uidensity", uidensity);
  }

  document.getElementById("bookmarks-view").place =
    "place:type=" + Ci.nsINavHistoryQueryOptions.RESULTS_AS_ROOTS_QUERY;
}

function searchBookmarks(aSearchString) {
  var tree = document.getElementById("bookmarks-view");
  if (!aSearchString) {
    // eslint-disable-next-line no-self-assign
    tree.place = tree.place;
  } else {
    Services.telemetry.keyedScalarAdd("sidebar.search", "bookmarks", 1);
    gCumulativeSearches++;
    tree.applyFilter(aSearchString, PlacesUtils.bookmarks.userContentRoots);
  }
}

function updateTelemetry(urlsOpened = []) {
  let searchesHistogram = Services.telemetry.getHistogramById(
    "PLACES_BOOKMARKS_SEARCHBAR_CUMULATIVE_SEARCHES"
  );
  searchesHistogram.add(gCumulativeSearches);
  clearCumulativeCounter();

  Services.telemetry.keyedScalarAdd(
    "sidebar.link",
    "bookmarks",
    urlsOpened.length
  );
}

function clearCumulativeCounter() {
  gCumulativeSearches = 0;
}

function unloadBookmarksSidebar() {
  clearCumulativeCounter();
  PlacesUIUtils.setMouseoverURL("", window);
}

window.addEventListener("SidebarFocused", () =>
  document.getElementById("search-box").focus()
);
