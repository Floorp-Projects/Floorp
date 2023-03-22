/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
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

var gHistoryTree;
var gSearchBox;
var gHistoryGrouping = "";
var gCumulativeSearches = 0;
var gCumulativeFilterCount = 0;

function HistorySidebarInit() {
  let uidensity = window.top.document.documentElement.getAttribute("uidensity");
  if (uidensity) {
    document.documentElement.setAttribute("uidensity", uidensity);
  }

  gHistoryTree = document.getElementById("historyTree");
  gSearchBox = document.getElementById("search-box");

  gHistoryGrouping = document
    .getElementById("viewButton")
    .getAttribute("selectedsort");

  this.groupHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_FILTER_TYPE"
  );
  this.groupHistogram.add(gHistoryGrouping);

  if (gHistoryGrouping == "site") {
    document.getElementById("bysite").setAttribute("checked", "true");
  } else if (gHistoryGrouping == "visited") {
    document.getElementById("byvisited").setAttribute("checked", "true");
  } else if (gHistoryGrouping == "lastvisited") {
    document.getElementById("bylastvisited").setAttribute("checked", "true");
  } else if (gHistoryGrouping == "dayandsite") {
    document.getElementById("bydayandsite").setAttribute("checked", "true");
  } else {
    document.getElementById("byday").setAttribute("checked", "true");
  }

  searchHistory("");
}

function GroupBy(groupingType) {
  if (groupingType != gHistoryGrouping) {
    this.groupHistogram.add(groupingType);
  }
  gHistoryGrouping = groupingType;
  gCumulativeFilterCount++;
  searchHistory(gSearchBox.value);
}

function updateTelemetry(urlsOpened = []) {
  let searchesHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_CUMULATIVE_SEARCHES"
  );
  searchesHistogram.add(gCumulativeSearches);
  let filterCountHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_CUMULATIVE_FILTER_COUNT"
  );
  filterCountHistogram.add(gCumulativeFilterCount);
  clearCumulativeCounters();

  Services.telemetry.keyedScalarAdd(
    "sidebar.link",
    "history",
    urlsOpened.length
  );
}

function searchHistory(aInput) {
  var query = PlacesUtils.history.getNewQuery();
  var options = PlacesUtils.history.getNewQueryOptions();

  const NHQO = Ci.nsINavHistoryQueryOptions;
  var sortingMode;
  var resultType;

  switch (gHistoryGrouping) {
    case "visited":
      resultType = NHQO.RESULTS_AS_URI;
      sortingMode = NHQO.SORT_BY_VISITCOUNT_DESCENDING;
      break;
    case "lastvisited":
      resultType = NHQO.RESULTS_AS_URI;
      sortingMode = NHQO.SORT_BY_DATE_DESCENDING;
      break;
    case "dayandsite":
      resultType = NHQO.RESULTS_AS_DATE_SITE_QUERY;
      break;
    case "site":
      resultType = NHQO.RESULTS_AS_SITE_QUERY;
      sortingMode = NHQO.SORT_BY_TITLE_ASCENDING;
      break;
    case "day":
    default:
      resultType = NHQO.RESULTS_AS_DATE_QUERY;
      break;
  }

  if (aInput) {
    query.searchTerms = aInput;
    if (gHistoryGrouping != "visited" && gHistoryGrouping != "lastvisited") {
      sortingMode = NHQO.SORT_BY_FRECENCY_DESCENDING;
      resultType = NHQO.RESULTS_AS_URI;
    }
  }

  options.sortingMode = sortingMode;
  options.resultType = resultType;
  options.includeHidden = !!aInput;

  if (gHistoryGrouping == "lastvisited") {
    TelemetryStopwatch.start("HISTORY_LASTVISITED_TREE_QUERY_TIME_MS");
  }

  // call load() on the tree manually
  // instead of setting the place attribute in historySidebar.xhtml
  // otherwise, we will end up calling load() twice
  gHistoryTree.load(query, options);

  // Sometimes search is activated without an input string. For example, when
  // the history sidbar is first opened or when a search filter is selected.
  // Since we're trying to measure how often the searchbar was used, we should first
  // check if there's an input string before collecting telemetry.
  if (aInput) {
    Services.telemetry.keyedScalarAdd("sidebar.search", "history", 1);
    gCumulativeSearches++;
  }

  if (gHistoryGrouping == "lastvisited") {
    TelemetryStopwatch.finish("HISTORY_LASTVISITED_TREE_QUERY_TIME_MS");
  }
}

function clearCumulativeCounters() {
  gCumulativeSearches = 0;
  gCumulativeFilterCount = 0;
}

function unloadHistorySidebar() {
  clearCumulativeCounters();
  PlacesUIUtils.setMouseoverURL("", window);
}

window.addEventListener("SidebarFocused", () => gSearchBox.focus());
