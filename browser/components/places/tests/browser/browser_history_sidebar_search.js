add_task(function* test () {
  let sidebar = document.getElementById("sidebar");

  // Visited pages listed by descending visit date.
  let pages = [
    "http://sidebar.mozilla.org/a",
    "http://sidebar.mozilla.org/b",
    "http://sidebar.mozilla.org/c",
    "http://www.mozilla.org/d",
  ];

  // Number of pages that will be filtered out by the search.
  const FILTERED_COUNT = 1;

  yield PlacesTestUtils.clearHistory();

  // Add some visited page.
  let time = Date.now();
  let places = [];
  for (let i = 0; i < pages.length; i++) {
    places.push({ uri: NetUtil.newURI(pages[i]),
                  visitDate: (time - i) * 1000,
                  transition: PlacesUtils.history.TRANSITION_TYPED });
  }
  yield PlacesTestUtils.addVisits(places);

  yield withSidebarTree("history", function* () {
    info("Set 'by last visited' view");
    sidebar.contentDocument.getElementById("bylastvisited").doCommand();
    let tree = sidebar.contentDocument.getElementById("historyTree");
    check_tree_order(tree, pages);

    // Set a search value.
    let searchBox = sidebar.contentDocument.getElementById("search-box");
    ok(searchBox, "search box is in context");
    searchBox.value = "sidebar.mozilla";
    searchBox.doCommand();
    check_tree_order(tree, pages, -FILTERED_COUNT);

    info("Reset the search");
    searchBox.value = "";
    searchBox.doCommand();
    check_tree_order(tree, pages);
  });

  yield PlacesTestUtils.clearHistory();
});

function check_tree_order(tree, pages, aNumberOfRowsDelta = 0) {
  let treeView = tree.view;
  let columns = tree.columns;
  is(columns.count, 1, "There should be only 1 column in the sidebar");

  let found = 0;
  for (let i = 0; i < treeView.rowCount; i++) {
    let node = treeView.nodeForTreeIndex(i);
    // We could inherit delayed visits from previous tests, skip them.
    if (!pages.includes(node.uri))
      continue;
    is(node.uri, pages[i], "Node is in correct position based on its visit date");
    found++;
  }
  ok(found, pages.length + aNumberOfRowsDelta, "Found all expected results");
}
