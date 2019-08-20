/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests the urlbar adaptive behavior powered by input history.
 */

"use strict";

async function bumpScore(uri, searchString, counts, useMouseClick = false) {
  if (counts.visits) {
    let visits = new Array(counts.visits).fill(uri);
    await PlacesTestUtils.addVisits(visits);
  }
  if (counts.picks) {
    for (let i = 0; i < counts.picks; ++i) {
      await promiseAutocompleteResultPopup(searchString);
      let promise = BrowserTestUtils.waitForDocLoadAndStopIt(
        uri,
        gBrowser.selectedBrowser
      );
      // Look for the expected uri.
      while (gURLBar.untrimmedValue != uri) {
        EventUtils.synthesizeKey("KEY_ArrowDown");
      }
      if (useMouseClick) {
        let element = UrlbarTestUtils.getSelectedElement(window);
        EventUtils.synthesizeMouseAtCenter(element, {});
      } else {
        EventUtils.synthesizeKey("KEY_Enter");
      }
      await promise;
    }
  }
  await PlacesTestUtils.promiseAsyncUpdates();
}

async function decayInputHistory() {
  PlacesUtils.history.decayFrecency();
  await PlacesTestUtils.promiseAsyncUpdates();
}

add_task(async function setup() {
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

add_task(async function test_adaptive_no_search_terms() {
  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, different picks");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 });
  await bumpScore(url2, "site", { visits: 3, picks: 1 });
  await promiseAutocompleteResultPopup("");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 1 });
  await bumpScore(url2, "site", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check second result");

  info("Different visit count, same picks");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 });
  await bumpScore(url2, "site", { visits: 1, picks: 3 });
  await promiseAutocompleteResultPopup("");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check second result");

  info("Different visit count, same picks, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 1, picks: 3 });
  await bumpScore(url2, "site", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check second result");
});

add_task(async function test_adaptive_with_search_terms() {
  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, same picks, one partial match, one exact match");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await bumpScore(url2, "site", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("si");
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
  await promiseAutocompleteResultPopup("si");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, different picks, both exact match");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await bumpScore(url2, "si", { visits: 3, picks: 1 });
  await promiseAutocompleteResultPopup("si");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, both exact match, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "si", { visits: 3, picks: 1 });
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("si");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, different picks, both partial match");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 });
  await bumpScore(url2, "site", { visits: 3, picks: 1 });
  await promiseAutocompleteResultPopup("si");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, both partial match, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 1 });
  await bumpScore(url2, "site", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("si");
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
  await promiseAutocompleteResultPopup("si");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, same picks, both exact match, decay second");
  await PlacesUtils.history.clear();
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await decayInputHistory();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("si");
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
  await promiseAutocompleteResultPopup("si");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");

  info("Same visit count, same picks, both exact match, decay second");
  await PlacesUtils.history.clear();
  await bumpScore(url2, "si", { visits: 3, picks: 3 });
  await decayInputHistory();
  await bumpScore(url1, "si", { visits: 3, picks: 3 });
  await promiseAutocompleteResultPopup("si");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");
});

add_task(async function test_adaptive_limited() {
  info(
    "Adaptive results should be added at the top up to maxRichResults / 4, then enqueued"
  );
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

  let expectedBookmarkIndex = Math.floor(n / 4) + 2;
  await promiseAutocompleteResultPopup("site");
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
  await bumpScore("http://site.tld/1", "site", { visits: 1, picks: 1 });

  let url = "http://bookmarked.site.tld/1";
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test_book",
    url,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Search only bookmarks.
      ["browser.urlbar.suggest.bookmarks", true],
      ["browser.urlbar.suggest.history", false],
    ],
  });
  await promiseAutocompleteResultPopup("site");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url, "Check bookmarked result");

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
  await PlacesUtils.history.clear();

  let url1 = "http://site.tld/1";
  let url2 = "http://site.tld/2";

  info("Same visit count, different picks");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 3 }, true);
  await bumpScore(url2, "site", { visits: 3, picks: 1 }, true);
  await promiseAutocompleteResultPopup("si");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url1, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url2, "Check second result");

  info("Same visit count, different picks, invert");
  await PlacesUtils.history.clear();
  await bumpScore(url1, "site", { visits: 3, picks: 1 }, true);
  await bumpScore(url2, "site", { visits: 3, picks: 3 }, true);
  await promiseAutocompleteResultPopup("si");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, url2, "Check first result");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 2);
  Assert.equal(result.url, url1, "Check second result");
});
