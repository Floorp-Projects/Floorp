/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

add_task(async function test_places_search_telemetry() {
  // Setup for test.
  await PlacesUtils.history.clear();
  let cumulativeSearchesHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_CUMULATIVE_SEARCHES"
  );
  cumulativeSearchesHistogram.clear();
  let cumulativeFilterCountHistogram = Services.telemetry.getHistogramById(
    "PLACES_SEARCHBAR_CUMULATIVE_FILTER_COUNT"
  );
  cumulativeFilterCountHistogram.clear();

  // Visited pages listed by descending visit date.
  let pages = [
    "https://sidebar.mozilla.org/a",
    "https://sidebar.mozilla.org/b",
    "https://sidebar.mozilla.org/c",
    "https://www.mozilla.org/d",
  ];
  const firstNodeIndex = 0;

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

  await withSidebarTree("history", async function() {
    // Check whether the history sidebar tree is visible before proceeding.
    let sidebar = window.SidebarUI.browser;
    let tree = sidebar.contentDocument.getElementById("historyTree");
    await TestUtils.waitForCondition(() =>
      tree.view.nodeForTreeIndex(firstNodeIndex)
    );
    info("Sidebar was opened and populated");

    // Apply a search filter.
    sidebar.contentDocument.getElementById("bylastvisited").doCommand();
    info("Search filter was changed to bylastvisited");

    // Search the tree.
    let searchBox = sidebar.contentDocument.getElementById("search-box");
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();
    info("Tree was searched with sting sidebar.mozilla");

    searchBox.value = "";
    searchBox.doCommand();
    info("Search was reset");

    // Perform a second search.
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();
    info("Second search was performed");

    // Select the first link and click on it.
    tree.selectNode(tree.view.nodeForTreeIndex(firstNodeIndex));
    synthesizeClickOnSelectedTreeCell(tree, { button: 1 });
    info("First link was selected and then clicked on");
  });

  TelemetryTestUtils.assertHistogram(cumulativeSearchesHistogram, 2, 1);
  info("Cumulative search telemetry looks right");

  TelemetryTestUtils.assertHistogram(cumulativeFilterCountHistogram, 1, 1);
  info("Cumulative search filter telemetry looks right");

  cumulativeSearchesHistogram.clear();
  cumulativeFilterCountHistogram.clear();

  await withSidebarTree("history", async function() {
    let sidebar = window.SidebarUI.browser;
    let tree = sidebar.contentDocument.getElementById("historyTree");
    await TestUtils.waitForCondition(() =>
      tree.view.nodeForTreeIndex(firstNodeIndex)
    );
    info("Sidebar was opened and populated");

    // Apply a search filter.
    sidebar.contentDocument.getElementById("byday").doCommand();
    info("First search filter applied");

    // Apply another search filter.
    sidebar.contentDocument.getElementById("bylastvisited").doCommand();
    info("Second search filter applied");

    // Apply a search filter.
    sidebar.contentDocument.getElementById("byday").doCommand();
    info("Third search filter applied");

    // Search the tree.
    let searchBox = sidebar.contentDocument.getElementById("search-box");
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();

    // Select the first link and click on it.
    tree.selectNode(tree.view.nodeForTreeIndex(firstNodeIndex));
    synthesizeClickOnSelectedTreeCell(tree, { button: 1 });
  });

  TelemetryTestUtils.assertHistogram(cumulativeSearchesHistogram, 1, 1);
  TelemetryTestUtils.assertHistogram(cumulativeFilterCountHistogram, 3, 1);

  await PlacesUtils.history.clear();
  cumulativeSearchesHistogram.clear();
  cumulativeFilterCountHistogram.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
