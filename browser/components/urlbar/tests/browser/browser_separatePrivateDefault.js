/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the 'Search in a Private Window' result of the address bar.
// Tests here don't have a different private engine, for that see
// browser_separatePrivateDefault_differentPrivateEngine.js

const serverInfo = {
  scheme: "http",
  host: "localhost",
  port: 20709, // Must be identical to what is in searchSuggestionEngine2.xml
};

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
  await Services.search.setDefaultPrivate(engine);

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

  registerCleanupFunction(async () => {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.setDefaultPrivate(oldDefaultPrivateEngine);
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
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, await Services.search.getDefault(), false);
});

add_task(async function test_search_urlbar_result_disabled() {
  info("Test that 'Search in a Private Window' does not appear when disabled");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.urlbarResult.enabled", false],
    ],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "unique198273982173",
  });
  await AssertNoPrivateResult(window);
  await SpecialPowers.popPrefEnv();
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
    value: "unique198273982173",
  });
  await AssertPrivateResult(window, await Services.search.getDefault(), false);
  await SpecialPowers.popPrefEnv();
});

// TODO: (Bug 1658620) Write a new subtest for this behaviour with the update2
// pref on.
// add_task(async function test_oneoff_selected_keyboard() {
//   info(
//     "Test that 'Search in a Private Window' with keyboard opens the selected one-off engine if there's no private engine"
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
//   await AssertPrivateResult(window, await Services.search.getDefault(), false);
//   // Select the 'Search in a Private Window' result, alt down to select the
//   // first one-off button, Enter. It should open a pb window, but using the
//   // selected one-off engine.
//   let promiseWindow = BrowserTestUtils.waitForNewWindow({
//     url:
//       "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/print_postdata.sjs",
//   });
//   // Select the private result.
//   EventUtils.synthesizeKey("KEY_ArrowDown");
//   // Select the first one-off button.
//   EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
//   EventUtils.synthesizeKey("VK_RETURN");
//   let win = await promiseWindow;
//   Assert.ok(
//     PrivateBrowsingUtils.isWindowPrivate(win),
//     "Should open a private window"
//   );
//   await BrowserTestUtils.closeWindow(win);
//   await SpecialPowers.popPrefEnv();
// });

// TODO: (Bug 1658620) Write a new subtest for this behaviour with the update2
// pref on.
// add_task(async function test_oneoff_selected_mouse() {
//   info(
//     "Test that 'Search in a Private Window' with mouse opens the selected one-off engine if there's no private engine"
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
//   await AssertPrivateResult(window, await Services.search.getDefault(), false);
//   // Select the 'Search in a Private Window' result, alt down to select the
//   // first one-off button, Enter. It should open a pb window, but using the
//   // selected one-off engine.
//   let promiseWindow = BrowserTestUtils.waitForNewWindow({
//     url:
//       "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/print_postdata.sjs",
//   });
//   // Select the private result.
//   EventUtils.synthesizeKey("KEY_ArrowDown");
//   // Select the first one-off button.
//   EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
//   // Click on the result.
//   let element = UrlbarTestUtils.getSelectedRow(window);
//   EventUtils.synthesizeMouseAtCenter(element, {});
//   let win = await promiseWindow;
//   Assert.ok(
//     PrivateBrowsingUtils.isWindowPrivate(win),
//     "Should open a private window"
//   );
//   await BrowserTestUtils.closeWindow(win);
//   await SpecialPowers.popPrefEnv();
// });
