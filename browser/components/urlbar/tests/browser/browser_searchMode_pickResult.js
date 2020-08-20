/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that search mode is exited after picking a result.
 */

"use strict";

const BOOKMARK_URL = "http://www.example.com/browser_searchMode_pickResult.js";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // Add a bookmark so we can enter bookmarks search mode and pick it.
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: BOOKMARK_URL,
  });
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

// Opens a new tab, enters search mode, does a search for our test bookmark, and
// picks it.  Uses a variety of initial URLs and search strings in order to hit
// different branches in setURI.  Search mode should be exited in all cases.
add_task(async function pickResult() {
  for (let test of [
    // initialURL, searchString
    ["about:blank", BOOKMARK_URL],
    ["about:blank", new URL(BOOKMARK_URL).origin],
    ["about:blank", new URL(BOOKMARK_URL).pathname],
    [BOOKMARK_URL, BOOKMARK_URL],
    [BOOKMARK_URL, new URL(BOOKMARK_URL).origin],
    [BOOKMARK_URL, new URL(BOOKMARK_URL).pathname],
  ]) {
    await doPickResultTest(...test);
  }
});

async function doPickResultTest(initialURL, searchString) {
  info(
    "doPickResultTest with args: " +
      JSON.stringify({
        initialURL,
        searchString,
      })
  );

  await BrowserTestUtils.withNewTab(initialURL, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    // Arrow down to the bookmark result.
    let foundResult = false;
    for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
      if (
        result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS &&
        result.url == BOOKMARK_URL
      ) {
        foundResult = true;
        break;
      }
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    Assert.ok(foundResult, "The bookmark result should have been found");

    // Press enter.
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;
    Assert.equal(
      gBrowser.currentURI.spec,
      BOOKMARK_URL,
      "Should have loaded the bookmarked URL"
    );

    await UrlbarTestUtils.assertSearchMode(window, null);
  });
}
