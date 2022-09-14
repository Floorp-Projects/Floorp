/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the 'Search in a Private Window' result of the address bar.

const serverInfo = {
  scheme: "http",
  host: "localhost",
  port: 20709, // Must be identical to what is in searchSuggestionEngine2.xml
};

let gAliasEngine;
let gPrivateEngine;

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault.urlbarResult.enabled", true],
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
  gPrivateEngine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine2.xml"
  );
  await Services.search.setDefaultPrivate(gPrivateEngine);

  // Add another engine in the first one-off position.
  let engine2 = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "POSTSearchEngine.xml"
  );
  await Services.search.moveEngine(engine2, 0);

  // Add an engine with an alias.
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch",
    keyword: "alias",
  });
  gAliasEngine = Services.search.getEngineByName("MozSearch");

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.setDefaultPrivate(oldDefaultPrivateEngine);
    await PlacesUtils.history.clear();
  });
});

async function AssertNoPrivateResult(win) {
  let count = await UrlbarTestUtils.getResultCount(win);
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

// Tests from here on have a different default private engine.

add_task(async function test_search_private_engine() {
  info(
    "Test that 'Search in a Private Window' reports a separate private engine"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, gPrivateEngine, true);
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

// TODO: (Bug 1658620) Write a new subtest for this behaviour with the update2
// pref on.
// add_task(async function test_oneoff_selected_with_private_engine_mouse() {
//   info(
//     "Test that 'Search in a Private Window' opens the private engine even if a one-off is selected"
//   );
//   await SpecialPowers.pushPrefEnv({
//     set: [
//       ["browser.urlbar.update2", false],
//       ["browser.urlbar.update2.oneOffsRefresh", false],
//     ],
//   });
//   await UrlbarTestUtils.promiseAutocompleteResultPopup({
//     window,
//     value: "unique198273982173",
//   });
//   await AssertPrivateResult(
//     window,
//     await Services.search.getDefaultPrivate(),
//     true
//   );

//   await withHttpServer(serverInfo, async () => {
//     // Select the 'Search in a Private Window' result, alt down to select the
//     // first one-off button, Click on the result. It should open a pb window using
//     // the private search engine, because it has been set.
//     let promiseWindow = BrowserTestUtils.waitForNewWindow({
//       url: "http://localhost:20709/?terms=unique198273982173",
//       maybeErrorPage: true,
//     });
//     // Select the private result.
//     EventUtils.synthesizeKey("KEY_ArrowDown");
//     // Select the first one-off button.
//     EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
//     // Click on the result.
//     let element = UrlbarTestUtils.getSelectedRow(window);
//     EventUtils.synthesizeMouseAtCenter(element, {});
//     let win = await promiseWindow;
//     Assert.ok(
//       PrivateBrowsingUtils.isWindowPrivate(win),
//       "Should open a private window"
//     );
//     await BrowserTestUtils.closeWindow(win);
//   });
//   await SpecialPowers.popPrefEnv();
// });

// TODO: (Bug 1658620) Write a new subtest for this behaviour with the update2
// pref on.
// add_task(async function test_oneoff_selected_with_private_engine_keyboard() {
//   info(
//     "Test that 'Search in a Private Window' opens the private engine even if a one-off is selected"
//   );
//   await SpecialPowers.pushPrefEnv({
//     set: [
//       ["browser.urlbar.update2", false],
//       ["browser.urlbar.update2.oneOffsRefresh", false],
//     ],
//   });
//   await UrlbarTestUtils.promiseAutocompleteResultPopup({
//     window,
//     value: "unique198273982173",
//   });
//   await AssertPrivateResult(
//     window,
//     await Services.search.getDefaultPrivate(),
//     true
//   );

//   await withHttpServer(serverInfo, async () => {
//     // Select the 'Search in a Private Window' result, alt down to select the
//     // first one-off button, Enter. It should open a pb window, but using the
//     // selected one-off engine.
//     let promiseWindow = BrowserTestUtils.waitForNewWindow({
//       url: "http://localhost:20709/?terms=unique198273982173",
//       maybeErrorPage: true,
//     });
//     // Select the private result.
//     EventUtils.synthesizeKey("KEY_ArrowDown");
//     // Select the first one-off button.
//     EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
//     EventUtils.synthesizeKey("VK_RETURN");
//     let win = await promiseWindow;
//     Assert.ok(
//       PrivateBrowsingUtils.isWindowPrivate(win),
//       "Should open a private window"
//     );
//     await BrowserTestUtils.closeWindow(win);
//   });
//   await SpecialPowers.popPrefEnv();
// });

add_task(async function test_alias_no_query() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2.emptySearchBehavior", 2]],
  });
  info(
    "Test that 'Search in a Private Window' doesn't appear if an alias is typed with no query"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "alias ",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: gAliasEngine.name,
    entry: "typed",
  });
  await AssertNoPrivateResult(window);
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_alias_query() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2.emptySearchBehavior", 2]],
  });
  info(
    "Test that 'Search in a Private Window' appears when an alias is typed with a query"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "alias something",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: "MozSearch",
    entry: "typed",
  });
  await AssertPrivateResult(window, gAliasEngine, true);
  await UrlbarTestUtils.exitSearchMode(window);
  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_restrict() {
  info(
    "Test that 'Search in a Private Window' doesn's appear for just the restriction token"
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: UrlbarTokenizer.RESTRICT.SEARCH,
  });
  await AssertNoPrivateResult(window);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: UrlbarTokenizer.RESTRICT.SEARCH + " ",
  });
  await AssertNoPrivateResult(window);
  await UrlbarTestUtils.exitSearchMode(window);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
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
    value: UrlbarTokenizer.RESTRICT.SEARCH + "test",
  });
  let result = await AssertPrivateResult(window, engine, true);
  Assert.equal(result.searchParams.query, "test");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test" + UrlbarTokenizer.RESTRICT.SEARCH,
  });
  result = await AssertPrivateResult(window, engine, true);
  Assert.equal(result.searchParams.query, "test");
});
