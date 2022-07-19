/* Any copyright is dedicated to the Public Domain.
 *  * http://creativecommons.org/publicdomain/zero/1.0/ */
/*
 * Test searching for the selected text using the context menu
 */

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

SearchTestUtils.init(this);

const ENGINE_NAME = "mozSearch";
const ENGINE_URL =
  "https://example.com/browser/browser/components/search/test/browser/mozsearch.sjs";

add_setup(async function() {
  await Services.search.init();

  await SearchTestUtils.installSearchExtension({
    name: ENGINE_NAME,
    search_url: ENGINE_URL,
    search_url_get_params: "test={searchTerms}",
  });

  let engine = await Services.search.getEngineByName(ENGINE_NAME);
  ok(engine, "Got a search engine");
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
  });
});

async function openNewSearchTab(event_args, expect_new_window = false) {
  // open context menu with right click
  let contextMenu = document.getElementById("contentAreaContextMenu");

  let popupPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupPromise;

  let searchItem = contextMenu.getElementsByAttribute(
    "id",
    "context-searchselect"
  )[0];

  // open new search tab with desired modifiers
  let searchTabPromise;
  if (expect_new_window) {
    searchTabPromise = BrowserTestUtils.waitForNewWindow({
      url: ENGINE_URL + "?test=test%2520search",
    });
  } else {
    searchTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      ENGINE_URL + "?test=test%2520search",
      true
    );
  }

  if ("button" in event_args) {
    // Bug 1704879: activateItem does not currently support button
    EventUtils.synthesizeMouseAtCenter(searchItem, event_args);
  } else {
    contextMenu.activateItem(searchItem, event_args);
  }

  if (expect_new_window) {
    let win = await searchTabPromise;
    return win.gBrowser.selectedTab;
  }
  return searchTabPromise;
}

add_task(async function test_whereToOpenLink() {
  // open search test page and select search text
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/search/test/browser/test_search.html"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [""], async function() {
    return new Promise(resolve => {
      content.document.addEventListener(
        "selectionchange",
        function() {
          resolve();
        },
        { once: true }
      );
      content.document.getSelection().selectAllChildren(content.document.body);
    });
  });

  // check where context search opens for different buttons/modifiers
  let searchTab = await openNewSearchTab({});
  is(
    searchTab,
    gBrowser.selectedTab,
    "Search tab is opened in foreground (no modifiers)"
  );
  BrowserTestUtils.removeTab(searchTab);

  // TODO bug 1704883: Re-enable this subtest. Native context menus on macOS do
  // not yet support alternate mouse buttons.
  if (
    !AppConstants.platform == "macosx" ||
    !Services.prefs.getBoolPref("widget.macos.native-context-menus", false)
  ) {
    searchTab = await openNewSearchTab({ button: 1 });
    isnot(
      searchTab,
      gBrowser.selectedTab,
      "Search tab is opened in background (middle mouse)"
    );
    BrowserTestUtils.removeTab(searchTab);
  }

  searchTab = await openNewSearchTab({ ctrlKey: true });
  isnot(
    searchTab,
    gBrowser.selectedTab,
    "Search tab is opened in background (Ctrl)"
  );
  BrowserTestUtils.removeTab(searchTab);

  let current_browser = gBrowser.selectedBrowser;
  searchTab = await openNewSearchTab({ shiftKey: true }, true);
  isnot(
    current_browser,
    gBrowser.getBrowserForTab(searchTab),
    "Search tab is opened in new window (Shift)"
  );
  BrowserTestUtils.removeTab(searchTab);

  info("flipping browser.search.context.loadInBackground and re-checking");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.context.loadInBackground", true]],
  });

  searchTab = await openNewSearchTab({});
  isnot(
    searchTab,
    gBrowser.selectedTab,
    "Search tab is opened in background (no modifiers)"
  );
  BrowserTestUtils.removeTab(searchTab);

  // TODO bug 1704883: Re-enable this subtest. Native context menus on macOS do
  // not yet support alternate mouse buttons.
  if (
    !AppConstants.platform == "macosx" ||
    !Services.prefs.getBoolPref("widget.macos.native-context-menus", false)
  ) {
    searchTab = await openNewSearchTab({ button: 1 });
    is(
      searchTab,
      gBrowser.selectedTab,
      "Search tab is opened in foreground (middle mouse)"
    );
    BrowserTestUtils.removeTab(searchTab);
  }

  searchTab = await openNewSearchTab({ ctrlKey: true });
  is(
    searchTab,
    gBrowser.selectedTab,
    "Search tab is opened in foreground (Ctrl)"
  );
  BrowserTestUtils.removeTab(searchTab);

  current_browser = gBrowser.selectedBrowser;
  searchTab = await openNewSearchTab({ shiftKey: true }, true);
  isnot(
    current_browser,
    gBrowser.getBrowserForTab(searchTab),
    "Search tab is opened in new window (Shift)"
  );
  BrowserTestUtils.removeTab(searchTab);

  // cleanup
  BrowserTestUtils.removeTab(tab);
});
