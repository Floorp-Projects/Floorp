/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests the urlbar adaptive behavior powered by input history.
 */

"use strict";

async function bumpScore(
  uri,
  searchString,
  counts,
  useMouseClick = false,
  needToLoad = false
) {
  if (counts.visits) {
    let visits = new Array(counts.visits).fill(uri);
    await PlacesTestUtils.addVisits(visits);
  }
  if (counts.picks) {
    for (let i = 0; i < counts.picks; ++i) {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: searchString,
      });
      let promise = needToLoad
        ? BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser)
        : BrowserTestUtils.waitForDocLoadAndStopIt(
            uri,
            gBrowser.selectedBrowser
          );
      // Look for the expected uri.
      while (gURLBar.untrimmedValue != uri) {
        EventUtils.synthesizeKey("KEY_ArrowDown", {});
      }
      if (useMouseClick) {
        let element = UrlbarTestUtils.getSelectedRow(window);
        EventUtils.synthesizeMouseAtCenter(element, {});
      } else {
        EventUtils.synthesizeKey("KEY_Enter", {});
      }
      await promise;
    }
  }
  await PlacesTestUtils.promiseAsyncUpdates();
}

async function decayInputHistory() {
  await Cc["@mozilla.org/places/frecency-recalculator;1"]
    .getService(Ci.nsIObserver)
    .wrappedJSObject.decay();
}

async function isPageInInputHistory(url) {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached(
    `SELECT 1
       FROM moz_inputhistory i
       JOIN moz_places h ON h.id = i.place_id
       WHERE h.url = :url`,
    { url }
  );
  return rows?.length > 0;
}

async function isInputHistoryUrlInResults(url) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); ++i) {
    const result = await UrlbarTestUtils.getRowAt(window, i).result;
    if (result.providerName == "InputHistory") {
      if (result.payload.url == url) {
        return true;
      }
    }
  }
  return false;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // We don't want autofill to influence this test.
      ["browser.urlbar.autoFill", false],
    ],
  });
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_adaptive_with_search_terms() {
  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, same picks, one partial match, one exact match");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await bumpScore(url2, "site", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info(
    "Same visit count, same picks, one partial match, one exact match, invert"
  );
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 });
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, different picks, both exact match");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await bumpScore(url2, "si", { visits: 3, picks: 1 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, both exact match, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 1 });
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, different picks, both partial match");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 });
  await bumpScore(url2, "site", { visits: 3, picks: 1 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, both partial match, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 1 });
  await bumpScore(url2, "site", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");
});

add_task(async function test_adaptive_with_decay() {
  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, same picks, both exact match, decay first");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await decayInputHistory();
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, same picks, both exact match, decay second");
  await PlacesUtils.history.clear();
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await decayInputHistory();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");
});

add_task(async function test_adaptive_limited() {
  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, same picks, both exact match, decay first");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await decayInputHistory();
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, same picks, both exact match, decay second");
  await PlacesUtils.history.clear();
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await decayInputHistory();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");
});

add_task(async function test_adaptive_limited() {
  info("Up to 3 adaptive results should be added at the top, then enqueued");
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  // Add as many adaptive results as maxRichResults.
  let n = UrlbarPrefs.get("maxRichResults");
  let urls = Array(n)
    .fill(0)
    .map((e, i) => "http://site.tld/" + i);
  for (let url of urls) {
    await bumpScore(url, "site", { visits: 1, picks: 1 });
  }

  // Add a matching bookmark with an higher frecency.
  let url = "http://site.bookmark.tld/";
  await PlacesTestUtils.addVisits(url);
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test_site_book",
    url,
  });

  // After 1 heuristic and 3 input history results.
  let expectedBookmarkIndex = 4;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "site",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    expectedBookmarkIndex
  );
  Assert.equal(result.url, url, "Check bookmarked result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, n - 1);
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    n,
    "Check all the results are filled"
  );
  Assert.ok(
    result.url.startsWith("http://site.tld"),
    "Check last adaptive result"
  );

  await PlacesUtils.bookmarks.remove(bm);
});

add_task(async function test_adaptive_behaviors() {
  info(
    "Check adaptive results are not provided regardless of the requested behavior"
  );
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  // Add an adaptive entry.
  let historyUrl = "http://site.tld/1";
  await bumpScore(historyUrl, "site", { visits: 1, picks: 1 });

  let bookmarkURL = "http://bookmarked.site.tld/1";
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test_book",
    url: bookmarkURL,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Search only bookmarks.
      ["browser.urlbar.suggest.bookmark", true],
      ["browser.urlbar.suggest.history", false],
    ],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "site",
  });
  let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(result.payload.url, bookmarkURL, "Check bookmarked result");
  Assert.notEqual(
    result.providerName,
    "InputHistory",
    "The bookmarked result is not from InputHistory."
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Check there are no unexpected results"
  );
  await PlacesUtils.bookmarks.remove(bm);

  // Repeat the previous case but now the bookmark has the same URL as the
  // history result. We expect the returned result comes from InputHistory.
  bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test_book",
    url: historyUrl,
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "sit",
  });
  result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(result.payload.url, historyUrl, "Check bookmarked result");
  Assert.equal(
    result.providerName,
    "InputHistory",
    "The bookmarked result is from InputHistory."
  );
  Assert.equal(
    result.source,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    "The input history result is a bookmark."
  );

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Check there are no unexpected results"
  );

  await SpecialPowers.popPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      // Search only open pages. We don't provide an open page, but we want to
      // enable at least one of these prefs so that UrlbarProviderInputHistory
      // is active.
      ["browser.urlbar.suggest.bookmark", false],
      ["browser.urlbar.suggest.history", false],
      ["browser.urlbar.suggest.openpage", true],
    ],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "site",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "There is no adaptive history result because it is not an open page."
  );
  await SpecialPowers.popPrefEnv();

  // Clearing history but not deleting the bookmark. This simulates the case
  // where the user has cleared their history or is using permanent private
  // browsing mode.
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.bookmark", true],
      ["browser.urlbar.suggest.history", false],
      ["browser.urlbar.suggest.openpage", false],
    ],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "sit",
  });
  result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
    .result;
  Assert.equal(result.payload.url, historyUrl, "Check bookmarked result");
  Assert.equal(
    result.providerName,
    "InputHistory",
    "The bookmarked result is from InputHistory."
  );
  Assert.equal(
    result.source,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    "The input history result is a bookmark."
  );

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Check there are no unexpected results"
  );

  await PlacesUtils.bookmarks.remove(bm);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_adaptive_mouse() {
  info("Check adaptive results are updated on mouse picks");
  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, different picks");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 }, true);
  await bumpScore(url2, "site", { visits: 3, picks: 1 }, true);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 1 }, true);
  await bumpScore(url2, "site", { visits: 3, picks: 3 }, true);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");
});

add_task(async function test_adaptive_searchmode() {
  info("Check adaptive history is not shown in search mode.");

  let suggestionsEngine = await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "searchSuggestionEngine.xml",
  });

  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Sanity check: adaptive history is shown for a normal search.");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 }, true);
  await bumpScore(url2, "site", { visits: 3, picks: 1 }, true);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Entering search mode.");
  // enterSearchMode checks internally that our site.tld URLs are not shown.
  await UrlbarTestUtils.enterSearchMode(window, {
    engineName: suggestionsEngine.name,
  });

  await Services.search.removeEngine(suggestionsEngine);
});

add_task(async function test_ignore_case() {
  const url1 = "http://example.com/yes";
  const url2 = "http://example.com/no";
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([url1, url2]);
  await UrlbarUtils.addToInputHistory(url1, "SampLE");
  await UrlbarUtils.addToInputHistory(url1, "SaMpLE");
  await UrlbarUtils.addToInputHistory(url1, "SAMPLE");
  await UrlbarUtils.addToInputHistory(url1, "sample");
  await UrlbarUtils.addToInputHistory(url2, "sample");
  await UrlbarUtils.addToInputHistory(url2, "sample");
  await UrlbarUtils.addToInputHistory(url2, "sample");
  await UrlbarUtils.addToInputHistory(url2, "sample");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "sAM",
  });
  const result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(
    result.url,
    url1,
    "Seaching for input history is case-insensitive"
  );
});

add_task(async function test_adaptive_history_in_privatewindow() {
  info(
    "Check adaptive history is not shown in private window as tab switching candidate."
  );

  await PlacesUtils.history.clear();

  info("Add a test url as an input history.");
  const url = "http://example.com/";
  // We need to wait for loading the page in order to register the url into
  // moz_openpages_temp table.
  await bumpScore(url, "exa", { visits: 1, picks: 1 }, false, true);

  info("Check the url could be registered properly.");
  const connection = await PlacesUtils.promiseLargeCacheDBConnection();
  const rows = await connection.executeCached(
    "SELECT userContextId FROM moz_openpages_temp WHERE url = :url",
    { url }
  );
  Assert.equal(rows.length, 1, "Length of rows for the url is 1.");
  Assert.greaterOrEqual(
    rows[0].getResultByName("userContextId"),
    0,
    "The url is registered as non-private-browsing context."
  );

  info("Open popup in private window.");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: privateWindow,
    value: "ex",
  });

  info("Check the popup results.");
  let hasResult = false;
  for (let i = 0; i < UrlbarTestUtils.getResultCount(privateWindow); i++) {
    const result = await UrlbarTestUtils.getDetailsOfResultAt(privateWindow, i);

    if (result.url !== url) {
      continue;
    }

    Assert.notEqual(
      result.type,
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      "Result type of the url is not for tab switch."
    );

    hasResult = true;
  }
  Assert.ok(hasResult, "Popup has a result for the url.");

  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_adaptive_dismiss() {
  info("Check dismissing an adaptive history result");

  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Sanity check: adaptive history is shown for a normal search.");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 }, true);
  await bumpScore(url2, "site", { visits: 3, picks: 1 }, true);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = UrlbarTestUtils.getRowAt(window, 1).result;
  Assert.equal(result.payload.url, url1, "Check result #1 URL");
  Assert.equal(result.providerName, "InputHistory", "Check result #1 provider");
  result = UrlbarTestUtils.getRowAt(window, 2).result;
  Assert.equal(result.payload.url, url2, "Check result #2 URL");
  Assert.equal(result.providerName, "InputHistory", "Check result #2 provider");
  let waitForHistoryRemoval =
    PlacesTestUtils.waitForNotification("page-removed");
  await UrlbarTestUtils.openResultMenuAndClickItem(window, "dismiss", {
    resultIndex: 1,
  });
  await waitForHistoryRemoval;
  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open after clicking the command"
  );

  Assert.ok(
    !(await isInputHistoryUrlInResults(url1)),
    "Check result has been removed"
  );
  Assert.strictEqual(
    await PlacesUtils.history.fetch(url1),
    null,
    "The removed page should not be in browsing history"
  );
  Assert.ok(
    !(await isPageInInputHistory(url1)),
    "The removed page should not be in input history"
  );

  Assert.ok(
    await isInputHistoryUrlInResults(url2),
    "Check result has been retained"
  );
  Assert.notStrictEqual(
    await PlacesUtils.history.fetch(url2),
    null,
    "The non removed page should still be in history"
  );
  Assert.ok(
    await isPageInInputHistory(url2),
    "The non removed page should still be in input history"
  );
});

add_task(async function test_bookmarked_adaptive_dismiss() {
  info("Check dismissing a bookmarked adaptive history result");

  let url = "http://mysite.tld/";

  info("Sanity check: adaptive history is shown for a normal search.");
  await PlacesUtils.history.clear();
  await bumpScore(url, "site", { visits: 3, picks: 3 }, true);
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "si",
  });
  let result = UrlbarTestUtils.getRowAt(window, 1).result;
  Assert.equal(result.payload.url, url, "Check result #1 URL");
  Assert.equal(result.providerName, "InputHistory", "Check result #1 provider");

  let waitForHistoryRemoval =
    PlacesTestUtils.waitForNotification("page-removed");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });
  await waitForHistoryRemoval;
  Assert.ok(
    gURLBar.view.isOpen,
    "The view should remain open after removing history"
  );

  Assert.ok(
    !(await isInputHistoryUrlInResults(url)),
    "Check result has been removed"
  );
  Assert.ok(
    !(await PlacesUtils.history.hasVisits(url)),
    "The removed page should not be in browsing history"
  );
  Assert.ok(
    !(await isPageInInputHistory(url)),
    "The removed page should be in input history"
  );
});
