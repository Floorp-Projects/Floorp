/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 392497 - search in history sidebar loses sort
 */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bh = hs.QueryInterface(Ci.nsIBrowserHistory);
var ios = Cc["@mozilla.org/network/io-service;1"].
          getService(Ci.nsIIOService);
function uri(spec) {
  return ios.newURI(spec, null, null);
}

var sidebar = document.getElementById("sidebar");

function add_visit(aURI, aDate) {
  var visitId = hs.addVisit(aURI,
                            aDate,
                            null, // no referrer
                            hs.TRANSITION_TYPED, // user typed in URL bar
                            false, // not redirect
                            0);
  return visitId;
}

// Visited pages listed by descending visit date.
var pages = [
  "http://sidebar.mozilla.org/a",
  "http://sidebar.mozilla.org/b",
  "http://sidebar.mozilla.org/c",
  "http://www.mozilla.org/d",
];
// Number of pages that will be filtered out by the search.
const FILTERED_COUNT = 1;

function test() {
  waitForExplicitFinish();

  // Cleanup.
  waitForClearHistory(continue_test);
}

function continue_test() {
  // Add some visited page.
  var time = Date.now();
  for (var i = 0; i < pages.length; i++) {
    add_visit(uri(pages[i]), (time - i) * 1000);
  }

  sidebar.addEventListener("load", function() {
    sidebar.removeEventListener("load", arguments.callee, true);
    executeSoon(function() {
      // Set "by last visited" in the sidebar (sort by visit date descendind).
      sidebar.contentDocument.getElementById("bylastvisited").doCommand();
      check_sidebar_tree_order(pages.length);
      var searchBox = sidebar.contentDocument.getElementById("search-box");
      ok(searchBox, "search box is in context");
      searchBox.value = "sidebar.mozilla";
      searchBox.doCommand();
      check_sidebar_tree_order(pages.length - FILTERED_COUNT);
      searchBox.value = "";
      searchBox.doCommand();
      check_sidebar_tree_order(pages.length);

      // Cleanup.
      toggleSidebar("viewHistorySidebar", false);
      waitForClearHistory(finish);
    });
  }, true);
  toggleSidebar("viewHistorySidebar", true);
}

function check_sidebar_tree_order(aExpectedRows) {
  var tree = sidebar.contentDocument.getElementById("historyTree");
  var treeView = tree.view;
  var rc = treeView.rowCount;
  var columns = tree.columns;
  is(columns.count, 1, "There should be only 1 column in the sidebar");
  var found = 0;
  for (var r = 0; r < rc; r++) {
    var node = treeView.nodeForTreeIndex(r);
    // We could inherit visits from previous tests, skip them since they are
    // not interesting for us.
    if (pages.indexOf(node.uri) == -1)
      continue;
    is(node.uri, pages[r], "Node is in correct position based on its visit date");
    found++;
  }
  ok(found, aExpectedRows, "Found all expected results");
}
