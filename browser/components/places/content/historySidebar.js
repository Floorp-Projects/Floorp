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
});

XPCOMUtils.defineLazyModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
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
    "HISTORY_SIDEBAR_VIEW_TYPE"
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

  // Needed due to Bug 1596852.
  // Should be removed once this bug is resolved.
  window.addEventListener(
    "pageshow",
    e => {
      window.windowGlobalChild.getActor("LightweightTheme").handleEvent(e);
    },
    { once: true }
  );
}

function GroupBy(groupingType) {
  if (groupingType != gHistoryGrouping) {
    this.groupHistogram.add(groupingType);
  }
  gHistoryGrouping = groupingType;
  searchHistory(gSearchBox.value);
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
  Services.telemetry.keyedScalarAdd("sidebar.search", "history", 1);
  gHistoryTree.load(query, options);

  if (gHistoryGrouping == "lastvisited") {
    TelemetryStopwatch.finish("HISTORY_LASTVISITED_TREE_QUERY_TIME_MS");
  }
}

window.addEventListener("SidebarFocused", () => gSearchBox.focus());
