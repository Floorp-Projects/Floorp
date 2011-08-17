/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
