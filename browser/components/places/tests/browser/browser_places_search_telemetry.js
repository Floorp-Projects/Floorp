/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

add_task(async function test_places_search_telemetry() {
  // Visited pages listed by descending visit date.
  let pages = [
    "https://sidebar.mozilla.org/a",
    "https://sidebar.mozilla.org/b",
    "https://sidebar.mozilla.org/c",
    "https://www.mozilla.org/d",
  ];

  await PlacesUtils.history.clear();

  // Add some visited page.
  let time = Date.now();
  let places = [];
  for (let i = 0; i < pages.length; i++) {
    places.push({
      uri: NetUtil.newURI(pages[i]),
      visitDate: (time - i) * 1000,
      transition: PlacesUtils.history.TRANSITION_TYPED,
    });
  }

  await PlacesTestUtils.addVisits(places);
  await withSidebarTree("history", function() {
    let sidebar = window.SidebarUI.browser;

    // Apply a search filter.
    sidebar.contentDocument.getElementById("bylastvisited").doCommand();
    let tree = sidebar.contentDocument.getElementById("historyTree");

    // Search the tree.
    let searchBox = sidebar.contentDocument.getElementById("search-box");
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();

    info("Reset the search");
    searchBox.value = "";
    searchBox.doCommand();

    // Perform a second search.
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();

    // Select the first link and click on it.
    const firstNodeIndex = 0;
    tree.selectNode(tree.view.nodeForTreeIndex(firstNodeIndex));
    synthesizeClickOnSelectedTreeCell(tree, { button: 1 });
  });

  let cumulativeSearchesHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_CUMULATIVE_SEARCHES"
  );
  TelemetryTestUtils.assertHistogram(cumulativeSearchesHistogram, 2, 1);

  let cumulativeFilterCountHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_CUMULATIVE_FILTER_COUNT"
  );
  TelemetryTestUtils.assertHistogram(cumulativeFilterCountHistogram, 1, 1);

  cumulativeSearchesHistogram.clear();
  cumulativeFilterCountHistogram.clear();

  await withSidebarTree("history", function() {
    let sidebar = window.SidebarUI.browser;
    let tree = sidebar.contentDocument.getElementById("historyTree");

    // Apply a search filter.
    sidebar.contentDocument.getElementById("byday").doCommand();

    // Apply another search filter.
    sidebar.contentDocument.getElementById("bylastvisited").doCommand();

    // Search the tree.
    let searchBox = sidebar.contentDocument.getElementById("search-box");
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();

    // Select the first link and click on it.
    const firstNodeIndex = 0;
    tree.selectNode(tree.view.nodeForTreeIndex(firstNodeIndex));
    synthesizeClickOnSelectedTreeCell(tree, { button: 1 });
  });

  TelemetryTestUtils.assertHistogram(cumulativeSearchesHistogram, 1, 1);
  TelemetryTestUtils.assertHistogram(cumulativeFilterCountHistogram, 2, 1);

  await PlacesUtils.history.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
