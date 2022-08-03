/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the view's QueryContextCache. When the view opens and a context is
// cached for the search, the view should *synchronously* open and update.

"use strict";

const TEST_URLS = [];
const TEST_URLS_COUNT = 5;
const TOP_SITES_VISIT_COUNT = 5;
const SEARCH_STRING = "example";

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_task(async function init() {
  // Clear history and bookmarks to make sure the URLs we add below are truly
  // the top sites. If any existing history or bookmarks were the top sites,
  // which is likely but not guaranteed, one or more "newtab-top-sites-changed"
  // notifications will be sent, potentially interfering with the rest of the
  // test. Waiting for Places updates to finish and then an extra tick should be
  // enough to make sure no more notfications occur.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.promiseAsyncUpdates();
  await TestUtils.waitForTick();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });

  // Add some URLs to populate both history and top sites. Each URL needs to
  // match `SEARCH_STRING`.
  for (let i = 0; i < TEST_URLS_COUNT; i++) {
    let url = `https://${i}.example.com/${SEARCH_STRING}`;
    TEST_URLS.unshift(url);
    // Each URL needs to be added several times to boost its frecency enough to
    // qualify as a top site.
    for (let j = 0; j < TOP_SITES_VISIT_COUNT; j++) {
      await PlacesTestUtils.addVisits(url);
    }
  }
  await updateTopSitesAndAwaitChanged(TEST_URLS_COUNT);

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function search() {
  await withNewBrowserWindow(async win => {
    // Do a search and then close the view.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: SEARCH_STRING,
    });
    await UrlbarTestUtils.promisePopupClose(win);

    // Open the view. It should open synchronously and the cached search context
    // should be used.
    await openViewAndAssertCached({
      win,
      searchString: SEARCH_STRING,
      cached: true,
    });
  });
});

add_task(async function topSites_simple() {
  await withNewBrowserWindow(async win => {
    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Open the view again. It should open synchronously and the cached
    // top-sites context should be used.
    await openViewAndAssertCached({ win, cached: true });
  });
});

add_task(async function topSites_nonEmptySearch() {
  await withNewBrowserWindow(async win => {
    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Do a search, close the view, and revert the input.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "test",
    });
    await UrlbarTestUtils.promisePopupClose(win);
    win.gURLBar.handleRevert();

    // Open the view. It should open synchronously and the cached top-sites
    // context should be used.
    await openViewAndAssertCached({ win, cached: true });
  });
});

add_task(async function topSites_otherEmptySearch() {
  await withNewBrowserWindow(async win => {
    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Enter search mode with an empty search string (by pressing accel+K),
    // starting a new search. The view should *not* open synchronously and the
    // cached top-sites context should not be used.
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("k", { accelKey: true }, win);
    Assert.ok(!win.gURLBar.view.isOpen, "View is not open");
    await searchPromise;
    await UrlbarTestUtils.assertSearchMode(win, {
      engineName: Services.search.defaultEngine.name,
      isGeneralPurposeEngine: true,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      isPreview: false,
      entry: "shortcut",
    });

    // Close the view and revert the input.
    await UrlbarTestUtils.promisePopupClose(win);
    win.gURLBar.handleRevert();
    await UrlbarTestUtils.assertSearchMode(win, null);

    // Open the view. It should open synchronously and the cached top-sites
    // context should be used.
    await openViewAndAssertCached({ win, cached: true });
  });
});

add_task(async function topSites_changed() {
  await withNewBrowserWindow(async win => {
    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Change the top sites by adding visits to a new URL.
    let newURL = "https://changed.example.com/";
    for (let j = 0; j < TOP_SITES_VISIT_COUNT; j++) {
      await PlacesTestUtils.addVisits(newURL);
    }
    await updateTopSitesAndAwaitChanged(TEST_URLS_COUNT + 1);

    // Open the view. It should *not* open synchronously and the cached
    // top-sites context should not be used.
    await openViewAndAssertCached({ win, cached: false });

    // Open the view again. It should open synchronously and the new cached
    // top-sites context with the new URL should be used.
    await openViewAndAssertCached({
      win,
      cached: true,
      urls: [newURL, ...TEST_URLS],
      // The new URL is sometimes at the end of the list of top sites instead of
      // the start, so ignore the order of the results.
      ignoreOrder: true,
    });

    // Remove the new URL. The top sites will update themselves automatically,
    // so we only need to wait for newtab-top-sites-changed.
    info("Removing new URL and awaiting newtab-top-sites-changed");
    let changedPromise = TestUtils.topicObserved("newtab-top-sites-changed");
    await PlacesUtils.history.remove([newURL]);
    await changedPromise;

    // Open the view. It should *not* open synchronously and the cached
    // top-sites context should not be used.
    await openViewAndAssertCached({ win, cached: false });

    // Open the view again. It should open synchronously and the new cached
    // top-sites context with the new URL should be used.
    await openViewAndAssertCached({ win, cached: true });
  });
});

add_task(async function topSites_disabled_1() {
  await withNewBrowserWindow(async win => {
    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Disable `browser.urlbar.suggest.topsites`.
    UrlbarPrefs.set("suggest.topsites", false);

    // Open the view. It should *not* open synchronously and the cached
    // top-sites context should not be used.
    await openViewAndAssertCached({
      win,
      cached: false,
      cachedAfterOpen: false,
    });

    // Clear the pref, open the view to show top sites, and close it.
    UrlbarPrefs.clear("suggest.topsites");
    await openViewAndAssertCached({ win, cached: false });

    // Open the view. It should open synchronously and the cached top-sites
    // context should be used.
    await openViewAndAssertCached({ win, cached: true });
  });
});

add_task(async function topSites_disabled_2() {
  await withNewBrowserWindow(async win => {
    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Disable `browser.newtabpage.activity-stream.feeds.system.topsites`.
    Services.prefs.setBoolPref(
      "browser.newtabpage.activity-stream.feeds.system.topsites",
      false
    );

    // Open the view. It should *not* open synchronously and the cached
    // top-sites context should not be used.
    await openViewAndAssertCached({
      win,
      cached: false,
      cachedAfterOpen: false,
    });

    // Clear the pref, open the view to show top sites, and close it.
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.system.topsites"
    );
    await openViewAndAssertCached({ win, cached: false });

    // Open the view. It should open synchronously and the cached top-sites
    // context should be used.
    await openViewAndAssertCached({ win, cached: true });
  });
});

add_task(async function evict() {
  await withNewBrowserWindow(async win => {
    let cache = win.gURLBar.view._queryContextCache;
    Assert.equal(
      typeof cache.size,
      "number",
      "Sanity check: _queryContextCache.size is a number"
    );

    // Open the view to show top sites and then close it.
    await openViewAndAssertCached({ win, cached: false });

    // Do `cache.size` + 1 searches.
    for (let i = 0; i < cache.size + 1; i++) {
      let searchString = "test" + i;
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window: win,
        value: searchString,
      });
      await UrlbarTestUtils.promisePopupClose(win);
      Assert.ok(
        cache.get(searchString),
        "Cache includes search string: " + searchString
      );
    }

    // The first search string should have been evicted from the cache, but the
    // one after that should still be cached.
    Assert.ok(!cache.get("test0"), "test0 has been evicted from the cache");
    Assert.ok(cache.get("test1"), "Cache includes test1");

    // Revert the input and open the view to show the top sites. It should open
    // synchronously and the cached top-sites context should be used.
    win.gURLBar.handleRevert();
    Assert.equal(win.gURLBar.value, "", "Input is empty after reverting");
    await openViewAndAssertCached({ win, cached: true });
  });
});

/**
 * Opens the view and checks that it is or is not synchronously opened and
 * populated as specified.
 *
 * @param {window} win
 * @param {boolean} cached
 *   Whether a query context is expected to already be cached for the search
 *   that's performed when the view opens. If true, then the view should
 *   synchronously open and populate using the cached context. If false, then
 *   the view should asynchronously open once the first results are fetched.
 * @param {boolean} cachedAfterOpen
 *   Whether the context is expected to be cached after the view opens and the
 *   query finishes.
 * @param {string} searchString
 *   The search string for which the context should or should not be cached. If
 *   falsey, then the relevant context is assumed to be the top-sites context.
 * @param {array} urls
 *   Array of URLs that are expected to be shown in the view.
 * @param {boolean} ignoreOrder
 *   Whether to treat `urls` as an unordered set instead of an array. When true,
 *   the order of results is ignored.
 */
async function openViewAndAssertCached({
  win,
  cached,
  cachedAfterOpen = true,
  searchString = "",
  urls = TEST_URLS,
  ignoreOrder = false,
}) {
  let cache = win.gURLBar.view._queryContextCache;
  let getContext = () =>
    searchString ? cache.get(searchString) : cache.topSitesContext;

  Assert.equal(
    !!getContext(),
    cached,
    "Context is present or not in cache as expected for search string: " +
      JSON.stringify(searchString)
  );

  // Open the view by performing the accel+L command.
  await SimpleTest.promiseFocus(win);
  win.document.getElementById("Browser:OpenLocation").doCommand();

  Assert.equal(
    win.gURLBar.view.isOpen,
    cached,
    "View is open or not as expected"
  );

  if (!cached && cachedAfterOpen) {
    // Wait for the search to finish and the context to be cached since callers
    // generally expect it.
    await TestUtils.waitForCondition(
      getContext,
      "Waiting for context to be cached for search string: " +
        JSON.stringify(searchString)
    );
  } else if (cached) {
    // The view is expected to open synchronously. Check the results. We don't
    // do this in the `!cached` case, when the view is expected to open
    // asynchronously, because there are plenty of other tests for that. Here we
    // want to make sure results are correct before the new search finishes in
    // order to avoid any flicker.
    let startIndex = 0;
    let resultCount = urls.length;
    if (searchString) {
      // Plus heuristic
      startIndex++;
      resultCount++;
    }
    Assert.equal(
      UrlbarTestUtils.getResultCount(win),
      resultCount,
      "View has expected result count"
    );
    for (let i = 0; i < urls.length; i++) {
      let details = await UrlbarTestUtils.getDetailsOfResultAt(
        win,
        i + startIndex
      );
      if (!ignoreOrder) {
        Assert.equal(
          details.url,
          urls[i],
          `Result at index ${i} has expected URL`
        );
      } else {
        Assert.ok(
          urls.includes(details.url),
          `Result URL at index ${i} is in expected URLs: ` + details.url
        );
      }
    }
  }

  // We await `lastQueryContextPromise` instead of the promise returned from
  // `UrlbarTestUtils.promiseSearchComplete()` because the latter assumes the
  // view will open, which isn't the case for every task here.
  await win.gURLBar.lastQueryContextPromise;
  await UrlbarTestUtils.promisePopupClose(win);
}

/**
 * Updates the top sites and waits for the "newtab-top-sites-changed"
 * notification. Note that this notification is not sent if the sites don't
 * actually change. In that case, use only `updateTopSites()` instead.
 *
 * @param {number} expectedCount
 *   The new expected number of top sites.
 */
async function updateTopSitesAndAwaitChanged(expectedCount) {
  info("Updating top sites and awaiting newtab-top-sites-changed");
  let changedPromise = TestUtils.topicObserved(
    "newtab-top-sites-changed"
  ).then(() => info("Observed newtab-top-sites-changed"));
  await updateTopSites(sites => sites?.length == expectedCount);
  await changedPromise;
}

async function withNewBrowserWindow(callback) {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await callback(win);
  await BrowserTestUtils.closeWindow(win);
}
