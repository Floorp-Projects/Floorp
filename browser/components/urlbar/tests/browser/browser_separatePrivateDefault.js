/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the 'Search in a Private Window' result of the address bar.

const serverInfo = {
  scheme: "http",
  host: "localhost",
  port: 20709, // Must be identical to what is in searchSuggestionEngine2.xml
};

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });

  // Add some history for the empty panel.
  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);

  // Add a search suggestion engine and move it to the front so that it appears
  // as the first one-off.
  let oldDefaultEngine = await Services.search.getDefault();
  let oldDefaultPrivateEngine = await Services.search.getDefaultPrivate();
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  await Services.search.setDefault(engine);
  await Services.search.setDefaultPrivate(engine);

  // Add another engine in the first one-off position.
  let engine2 = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "POSTSearchEngine.xml"
  );
  await Services.search.moveEngine(engine2, 0);

  // Add an engine with an alias.
  let aliasEngine = await Services.search.addEngineWithDetails("MozSearch", {
    alias: "alias",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.setDefaultPrivate(oldDefaultPrivateEngine);
    await Services.search.removeEngine(aliasEngine);
    await PlacesUtils.history.clear();
  });
});

async function AssertNoPrivateResult(win) {
  let count = await UrlbarTestUtils.getResultCount(win);
  Assert.ok(count > 0, "Sanity check result count");
  for (let i = 0; i < count; ++i) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(win, i);
    Assert.ok(
      result.type != UrlbarUtils.RESULT_TYPE.SEARCH ||
        !result.searchParams.inPrivateWindow,
      "Check this result is not a 'Search in a Private Window' one"
    );
  }
}

async function AssertPrivateResult(win, engine, isPrivateEngine) {
  let count = await UrlbarTestUtils.getResultCount(win);
  Assert.ok(count > 1, "Sanity check result count");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Check result type"
  );
  Assert.ok(result.searchParams.inPrivateWindow, "Check inPrivateWindow");
  Assert.equal(
    result.searchParams.isPrivateEngine,
    isPrivateEngine,
    "Check isPrivateEngine"
  );
  Assert.equal(
    result.searchParams.engine,
    engine.name,
    "Check the search engine"
  );
  return result;
}

add_task(async function test_nonsearch() {
  info(
    "Test that 'Search in a Private Window' does not appear with non-search results"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exa",
  });
  await AssertNoPrivateResult(window);
});

add_task(async function test_search() {
  info(
    "Test that 'Search in a Private Window' appears with only search results"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, await Services.search.getDefault(), false);
});

add_task(async function test_search_disabled_suggestions() {
  info(
    "Test that 'Search in a Private Window' appears if suggestions are disabled"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", false]],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, await Services.search.getDefault(), false);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_oneoff_selected_keyboard() {
  info(
    "Test that 'Search in a Private Window' with keyboard opens the selected one-off engine if there's no private engine"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, await Services.search.getDefault(), false);
  // Select the 'Search in a Private Window' result, alt down to select the
  // first one-off button, Enter. It should open a pb window, but using the
  // selected one-off engine.
  let promiseWindow = BrowserTestUtils.waitForNewWindow({
    url:
      "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/print_postdata.sjs",
  });
  // Select the private result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  // Select the first one-off button.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  EventUtils.synthesizeKey("VK_RETURN");
  let win = await promiseWindow;
  Assert.ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "Should open a private window"
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_oneoff_selected_mouse() {
  info(
    "Test that 'Search in a Private Window' with mouse opens the selected one-off engine if there's no private engine"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, await Services.search.getDefault(), false);
  // Select the 'Search in a Private Window' result, alt down to select the
  // first one-off button, Enter. It should open a pb window, but using the
  // selected one-off engine.
  let promiseWindow = BrowserTestUtils.waitForNewWindow({
    url:
      "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/print_postdata.sjs",
  });
  // Select the private result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  // Select the first one-off button.
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  // Click on the result.
  let element = UrlbarTestUtils.getSelectedRow(window);
  EventUtils.synthesizeMouseAtCenter(element, {});
  let win = await promiseWindow;
  Assert.ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "Should open a private window"
  );
  await BrowserTestUtils.closeWindow(win);
});

// Tests from here on have a different default private engine.

add_task(async function test_search_private_engine() {
  info(
    "Test that 'Search in a Private Window' reports a separate private engine"
  );
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine2.xml"
  );
  await Services.search.setDefaultPrivate(engine);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, engine, true);
});

add_task(async function test_privateWindow() {
  info(
    "Test that 'Search in a Private Window' does not appear in a private window"
  );
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: privateWin,
    waitForFocus: SimpleTest.waitForFocus,
    value: "unique198273982173",
  });
  await AssertNoPrivateResult(privateWin);
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function test_permanentPB() {
  info(
    "Test that 'Search in a Private Window' does not appear in Permanent Private Browsing"
  );
  await SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.autostart", true]],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    waitForFocus: SimpleTest.waitForFocus,
    value: "unique198273982173",
  });
  await AssertNoPrivateResult(win);
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_openPBWindow() {
  info(
    "Test that 'Search in a Private Window' opens the search in a new Private Window"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(
    window,
    await Services.search.getDefaultPrivate(),
    true
  );

  await withHttpServer(serverInfo, async () => {
    let promiseWindow = BrowserTestUtils.waitForNewWindow({
      url: "http://localhost:20709/?terms=unique198273982173",
      maybeErrorPage: true,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("VK_RETURN");
    let win = await promiseWindow;
    Assert.ok(
      PrivateBrowsingUtils.isWindowPrivate(win),
      "Should open a private window"
    );
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_oneoff_selected_with_private_engine_mouse() {
  info(
    "Test that 'Search in a Private Window' opens the private engine even if a one-off is selected"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(
    window,
    await Services.search.getDefaultPrivate(),
    true
  );

  await withHttpServer(serverInfo, async () => {
    // Select the 'Search in a Private Window' result, alt down to select the
    // first one-off button, Click on the result. It should open a pb window using
    // the private search engine, because it has been set.
    let promiseWindow = BrowserTestUtils.waitForNewWindow({
      url: "http://localhost:20709/?terms=unique198273982173",
      maybeErrorPage: true,
    });
    // Select the private result.
    EventUtils.synthesizeKey("KEY_ArrowDown");
    // Select the first one-off button.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    // Click on the result.
    let element = UrlbarTestUtils.getSelectedRow(window);
    EventUtils.synthesizeMouseAtCenter(element, {});
    let win = await promiseWindow;
    Assert.ok(
      PrivateBrowsingUtils.isWindowPrivate(win),
      "Should open a private window"
    );
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_oneoff_selected_with_private_engine_keyboard() {
  info(
    "Test that 'Search in a Private Window' opens the private engine even if a one-off is selected"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "unique198273982173",
  });
  await AssertPrivateResult(
    window,
    await Services.search.getDefaultPrivate(),
    true
  );

  await withHttpServer(serverInfo, async () => {
    // Select the 'Search in a Private Window' result, alt down to select the
    // first one-off button, Enter. It should open a pb window, but using the
    // selected one-off engine.
    let promiseWindow = BrowserTestUtils.waitForNewWindow({
      url: "http://localhost:20709/?terms=unique198273982173",
      maybeErrorPage: true,
    });
    // Select the private result.
    EventUtils.synthesizeKey("KEY_ArrowDown");
    // Select the first one-off button.
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("VK_RETURN");
    let win = await promiseWindow;
    Assert.ok(
      PrivateBrowsingUtils.isWindowPrivate(win),
      "Should open a private window"
    );
    await BrowserTestUtils.closeWindow(win);
  });
});

add_task(async function test_alias() {
  info(
    "Test that 'Search in a Private Window' doesn't appear if an alias is typed"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "alias",
  });
  await AssertNoPrivateResult(window);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "alias something",
  });
  await AssertNoPrivateResult(window);
});

add_task(async function test_restrict() {
  info(
    "Test that 'Search in a Private Window' doesn's appear for just the restriction token"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: UrlbarTokenizer.RESTRICT.SEARCH,
  });
  await AssertNoPrivateResult(window);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: UrlbarTokenizer.RESTRICT.SEARCH + " ",
  });
  await AssertNoPrivateResult(window);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: " " + UrlbarTokenizer.RESTRICT.SEARCH,
  });
  await AssertNoPrivateResult(window);
});

add_task(async function test_restrict_search() {
  info(
    "Test that 'Search in a Private Window' has the right string with the restriction token"
  );
  let engine = await Services.search.getDefaultPrivate();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: UrlbarTokenizer.RESTRICT.SEARCH + "test",
  });
  let result = await AssertPrivateResult(window, engine, true);
  Assert.equal(result.searchParams.query, "test");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test" + UrlbarTokenizer.RESTRICT.SEARCH,
  });
  result = await AssertPrivateResult(window, engine, true);
  Assert.equal(result.searchParams.query, "test");
});
