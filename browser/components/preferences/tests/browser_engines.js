const { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);

SearchTestUtils.init(this);

function getCellText(tree, i, cellName) {
  return tree.view.getCellText(i, tree.columns.getNamedColumn(cellName));
}

add_task(async function setup() {
  await SearchTestUtils.installSearchExtension({
    keyword: ["testing", "customkeyword"],
    search_url: "https://example.com/engine1",
    search_url_get_params: "search={searchTerms}",
  });
});

// Test Engine list
add_task(async function() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("search", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneSearch", "Search pane is selected by default");
  let doc = gBrowser.contentDocument;

  let tree = doc.querySelector("#engineList");
  ok(
    !tree.hidden,
    "The search engine list should be visible when Search is requested"
  );

  // Check for default search engines to be displayed in the engineList
  let defaultEngines = await Services.search.getAppProvidedEngines();
  for (let i = 0; i < defaultEngines.length; i++) {
    let engine = defaultEngines[i];
    is(
      getCellText(tree, i, "engineName"),
      engine.name,
      "Default search engine " + engine.name + " displayed correctly"
    );
  }

  let customEngineIndex = defaultEngines.length;
  is(
    getCellText(tree, customEngineIndex, "engineKeyword"),
    "testing, customkeyword",
    "Show internal aliases"
  );

  // Scroll the treeview into view since mouse operations
  // off screen can act confusingly.
  tree.scrollIntoView();
  let rect = tree.getCoordsForCellItem(
    customEngineIndex,
    tree.columns.getNamedColumn("engineKeyword"),
    "text"
  );
  let x = rect.x + rect.width / 2;
  let y = rect.y + rect.height / 2;
  let win = tree.ownerGlobal;

  let promise = BrowserTestUtils.waitForEvent(tree, "dblclick");
  EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 1 }, win);
  EventUtils.synthesizeMouse(tree.body, x, y, { clickCount: 2 }, win);
  await promise;

  EventUtils.sendString("newkeyword");
  EventUtils.sendKey("RETURN");

  await TestUtils.waitForCondition(() => {
    return (
      getCellText(tree, customEngineIndex, "engineKeyword") ===
      "newkeyword, testing, customkeyword"
    );
  });

  // Avoid duplicated keywords
  tree.view.setCellText(
    0,
    tree.columns.getNamedColumn("engineKeyword"),
    "keyword"
  );
  tree.view.setCellText(
    1,
    tree.columns.getNamedColumn("engineKeyword"),
    "keyword"
  );
  isnot(
    getCellText(tree, 1, "engineKeyword"),
    "keyword",
    "Do not allow duplicated keywords"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
