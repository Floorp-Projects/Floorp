/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests checks that search suggestions can be acted upon correctly
 * e.g. selection with modifiers, copying text.
 */

"use strict";

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

const MAX_CHARS_PREF = "browser.urlbar.maxCharsForSearchSuggestions";

// Must run first.
add_task(async function prepare() {
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    setAsDefault: true,
  });
  await UrlbarTestUtils.formHistory.clear();
  registerCleanupFunction(async function () {
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);

    // Clicking suggestions causes visits to search results pages, so clear that
    // history now.
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function clickSuggestion() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let [idx, suggestion, engineName] = await getFirstSuggestion();
  Assert.equal(
    engineName,
    "browser_searchSuggestionEngine searchSuggestionEngine.xml",
    "Expected suggestion engine"
  );

  let uri = (await Services.search.getDefault()).getSubmission(suggestion).uri;
  let loadPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    uri.spec
  );
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, idx);
  EventUtils.synthesizeMouseAtCenter(element, {}, window);
  await loadPromise;

  let formHistory = (
    await UrlbarTestUtils.formHistory.search({ source: engineName })
  ).map(entry => entry.value);
  Assert.deepEqual(
    formHistory,
    ["foofoo"],
    "Should find form history after adding it"
  );

  BrowserTestUtils.removeTab(tab);
  await UrlbarTestUtils.formHistory.clear();
});

async function testPressEnterOnSuggestion(
  expectedUrl = null,
  keyModifiers = {}
) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let [idx, suggestion, engineName] = await getFirstSuggestion();
  Assert.equal(
    engineName,
    "browser_searchSuggestionEngine searchSuggestionEngine.xml",
    "Expected suggestion engine"
  );

  let hasExpectedUrl = !!expectedUrl;
  if (!expectedUrl) {
    expectedUrl = (await Services.search.getDefault()).getSubmission(suggestion)
      .uri.spec;
  }

  let promiseLoad = BrowserTestUtils.waitForDocLoadAndStopIt(
    expectedUrl,
    gBrowser.selectedBrowser
  );

  let promiseFormHistory;
  if (!hasExpectedUrl) {
    promiseFormHistory = UrlbarTestUtils.formHistory.promiseChanged("add");
  }

  for (let i = 0; i < idx; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  EventUtils.synthesizeKey("KEY_Enter", keyModifiers);

  await promiseLoad;

  if (!hasExpectedUrl) {
    await promiseFormHistory;
    let formHistory = (
      await UrlbarTestUtils.formHistory.search({ source: engineName })
    ).map(entry => entry.value);
    Assert.deepEqual(
      formHistory,
      ["foofoo"],
      "Should find form history after adding it"
    );
  }

  BrowserTestUtils.removeTab(tab);
  await UrlbarTestUtils.formHistory.clear();
}

add_task(async function plainEnterOnSuggestion() {
  await testPressEnterOnSuggestion();
});

add_task(async function ctrlEnterOnSuggestion() {
  await testPressEnterOnSuggestion("https://www.foofoo.com/", {
    ctrlKey: true,
  });
});

add_task(async function copySuggestionText() {
  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  let [idx, suggestion] = await getFirstSuggestion();
  for (let i = 0; i < idx; ++i) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
  }
  gURLBar.select();
  await SimpleTest.promiseClipboardChange(suggestion, () => {
    goDoCommand("cmd_copy");
  });
});

add_task(async function typeMaxChars() {
  gURLBar.focus();

  let maxChars = 10;
  await SpecialPowers.pushPrefEnv({
    set: [[MAX_CHARS_PREF, maxChars]],
  });

  // Make a string as long as maxChars and type it.
  let value = "";
  for (let i = 0; i < maxChars; i++) {
    value += String.fromCharCode("a".charCodeAt(0) + i);
  }
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
  });

  // Suggestions should be fetched since we allow them when typing, and the
  // value so far isn't longer than maxChars anyway.
  await assertSuggestions([value + "foo", value + "bar"]);

  // Now type some additional chars.  Suggestions should still be fetched since
  // we allow them when typing.
  for (let i = 0; i < 3; i++) {
    let char = String.fromCharCode("z".charCodeAt(0) - i);
    value += char;
    EventUtils.synthesizeKey(char);
    await assertSuggestions([value + "foo", value + "bar"]);
  }

  await SpecialPowers.popPrefEnv();
});

add_task(async function pasteMaxChars() {
  gURLBar.focus();

  let maxChars = 10;
  await SpecialPowers.pushPrefEnv({
    set: [[MAX_CHARS_PREF, maxChars]],
  });

  // Make a string as long as maxChars and paste it.
  let value = "";
  for (let i = 0; i < maxChars; i++) {
    value += String.fromCharCode("a".charCodeAt(0) + i);
  }
  await selectAndPaste(value);

  // Suggestions should be fetched since the pasted string is not longer than
  // maxChars.
  await assertSuggestions([value + "foo", value + "bar"]);

  // Now type some additional chars.  Suggestions should still be fetched since
  // we allow them when typing.
  for (let i = 0; i < 3; i++) {
    let char = String.fromCharCode("z".charCodeAt(0) - i);
    value += char;
    EventUtils.synthesizeKey(char);
    await assertSuggestions([value + "foo", value + "bar"]);
  }

  await SpecialPowers.popPrefEnv();
});

add_task(async function pasteMoreThanMaxChars() {
  gURLBar.focus();

  let maxChars = 10;
  await SpecialPowers.pushPrefEnv({
    set: [[MAX_CHARS_PREF, maxChars]],
  });

  // Make a string longer than maxChars and paste it.
  let value = "";
  for (let i = 0; i < 2 * maxChars; i++) {
    value += String.fromCharCode("a".charCodeAt(0) + i);
  }
  await selectAndPaste(value);

  // Suggestions should not be fetched since the value was pasted and it was
  // longer than maxChars.
  await assertSuggestions([]);

  // Now type some additional chars.  Suggestions should now be fetched since we
  // allow them when typing.
  for (let i = 0; i < 3; i++) {
    let char = String.fromCharCode("z".charCodeAt(0) - i);
    value += char;
    EventUtils.synthesizeKey(char);
    await assertSuggestions([value + "foo", value + "bar"]);
  }

  // Paste again.  The string is longer than maxChars, so suggestions should not
  // be fetched.
  await selectAndPaste(value);
  await assertSuggestions([]);

  await SpecialPowers.popPrefEnv();
});

add_task(async function heuristicAddsFormHistory() {
  await UrlbarTestUtils.formHistory.clear();
  let formHistory = (await UrlbarTestUtils.formHistory.search()).map(
    entry => entry.value
  );
  Assert.deepEqual(formHistory, [], "Form history should be empty initially");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(result.heuristic);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
  Assert.equal(result.searchParams.query, "foo");

  let uri = (await Services.search.getDefault()).getSubmission("foo").uri;
  let loadPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    uri.spec
  );
  let formHistoryPromise = UrlbarTestUtils.formHistory.promiseChanged("add");
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(element, {}, window);
  await loadPromise;

  await formHistoryPromise;
  formHistory = (
    await UrlbarTestUtils.formHistory.search({
      source: result.searchParams.engine,
    })
  ).map(entry => entry.value);
  Assert.deepEqual(
    formHistory,
    ["foo"],
    "Should find form history after adding it"
  );

  BrowserTestUtils.removeTab(tab);
  await UrlbarTestUtils.formHistory.clear();
});

add_task(async function minChars() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.trending.featureGate", true]],
  });
  gURLBar.focus();

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "f",
  });

  // Suggestions shouldn't be fetched with single character queries.
  await assertSuggestions([]);
  await SpecialPowers.popPrefEnv();
});

async function getFirstSuggestion() {
  let results = await getSuggestionResults();
  if (!results.length) {
    return [-1, null, null];
  }
  let result = results[0];
  return [
    result.index,
    result.searchParams.suggestion,
    result.searchParams.engine,
  ];
}

async function getSuggestionResults() {
  await UrlbarTestUtils.promiseSearchComplete(window);

  let results = [];
  let matchCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < matchCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.searchParams.suggestion
    ) {
      result.index = i;
      results.push(result);
    }
  }
  return results;
}

async function assertSuggestions(expectedSuggestions) {
  let results = await getSuggestionResults();
  let actualSuggestions = results.map(r => r.searchParams.suggestion);
  Assert.deepEqual(
    actualSuggestions,
    expectedSuggestions,
    "Expected suggestions"
  );
}
