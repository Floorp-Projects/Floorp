// Test Engine list
add_task(async function() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("general-search", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default");
  let doc = gBrowser.contentDocument;

  let tree = doc.querySelector("#engineList");
  ok(!tree.hidden, "The search engine list should be visible when Search is requested");

  // Check for default search engines to be displayed in the engineList
  let defaultEngines = Services.search.getDefaultEngines();
  for (let i = 0; i < defaultEngines.length; i++) {
      let engine = defaultEngines[i];
      let cellName = tree.view.getCellText(i, tree.columns.getNamedColumn("engineName"));
      is(cellName, engine.name, "Default search engine " + engine.name + " displayed correctly");
  }

  // Avoid duplicated keywords
  tree.view.setCellText(0, tree.columns.getNamedColumn("engineKeyword"), "keyword");
  tree.view.setCellText(1, tree.columns.getNamedColumn("engineKeyword"), "keyword");
  let cellKeyword = tree.view.getCellText(1, tree.columns.getNamedColumn("engineKeyword"));
  isnot(cellKeyword, "keyword", "Do not allow duplicated keywords");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

