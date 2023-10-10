/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests what happens when the enter key is pressed quickly after entering text.
 */

// The order of these tests matters!
const IS_UPGRADING_SCHEMELESS = SpecialPowers.getBoolPref(
  "dom.security.https_first_schemeless"
);
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const DEFAULT_URL_SCHEME = IS_UPGRADING_SCHEMELESS ? "https://" : "http://";

add_setup(async function () {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: DEFAULT_URL_SCHEME + "/example.com/?q=%s",
    title: "test",
  });
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
    await PlacesUtils.history.clear();
  });
  // Needs at least one success.
  ok(true, "Setup complete");
});

add_task(
  taskWithNewTab(async function test_loadSite() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.autofill", false]],
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "example.co",
    });
    gURLBar.focus();
    EventUtils.sendString("m");
    EventUtils.synthesizeKey("KEY_Enter");
    info("wait for the page to load");
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedTab.linkedBrowser,
      false,
      DEFAULT_URL_SCHEME + "example.com/"
    );
    await SpecialPowers.popPrefEnv();
  })
);

add_task(
  taskWithNewTab(async function test_sametext() {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "example.com",
      fireInputEvent: true,
    });

    // Simulate re-entering the same text searched the last time. This may happen
    // through a copy paste, but clipboard handling is not much reliable, so just
    // fire an input event.
    info("synthesize input event");
    let event = document.createEvent("Events");
    event.initEvent("input", true, true);
    gURLBar.inputField.dispatchEvent(event);
    EventUtils.synthesizeKey("KEY_Enter");

    info("wait for the page to load");
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedTab.linkedBrowser,
      false,
      DEFAULT_URL_SCHEME + "example.com/"
    );
  })
);

add_task(
  taskWithNewTab(async function test_after_empty_search() {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    gURLBar.focus();
    gURLBar.value = "e";
    EventUtils.synthesizeKey("x");
    EventUtils.synthesizeKey("KEY_Enter");

    info("wait for the page to load");
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedTab.linkedBrowser,
      false,
      DEFAULT_URL_SCHEME + "example.com/"
    );
  })
);

add_task(
  taskWithNewTab(async function test_disabled_ac() {
    // Disable autocomplete.
    let suggestHistory = Preferences.get("browser.urlbar.suggest.history");
    Preferences.set("browser.urlbar.suggest.history", false);
    let suggestBookmarks = Preferences.get("browser.urlbar.suggest.bookmark");
    Preferences.set("browser.urlbar.suggest.bookmark", false);
    let suggestOpenPages = Preferences.get("browser.urlbar.suggest.openpage");
    Preferences.set("browser.urlbar.suggest.openpage", false);

    await SearchTestUtils.installSearchExtension();

    let engine = Services.search.getEngineByName("Example");
    let originalEngine = await Services.search.getDefault();
    await Services.search.setDefault(
      engine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );

    async function cleanup() {
      Preferences.set("browser.urlbar.suggest.history", suggestHistory);
      Preferences.set("browser.urlbar.suggest.bookmark", suggestBookmarks);
      Preferences.set("browser.urlbar.suggest.openpage", suggestOpenPages);

      await Services.search.setDefault(
        originalEngine,
        Ci.nsISearchService.CHANGE_REASON_UNKNOWN
      );
    }
    registerCleanupFunction(cleanup);

    gURLBar.focus();
    gURLBar.value = "e";
    EventUtils.sendString("x");
    EventUtils.synthesizeKey("KEY_Enter");

    info("wait for the page to load");
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedTab.linkedBrowser,
      false,
      "https://example.com/?q=ex"
    );
    await cleanup();
  })
);

// Tests that setting a high value for browser.urlbar.delay does not delay the
// fetching of heuristic results.
add_task(
  taskWithNewTab(async function test_delay() {
    // This is needed to clear the current value, otherwise autocomplete may think
    // the user removed text from the end.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });
    await UrlbarTestUtils.promisePopupClose(window);

    // Set a large delay.
    const TIMEOUT = 3000;
    let delay = UrlbarPrefs.get("delay");
    UrlbarPrefs.set("delay", TIMEOUT);
    registerCleanupFunction(function () {
      UrlbarPrefs.set("delay", delay);
    });

    gURLBar.focus();
    gURLBar.value = "e";
    let recievedResult = new Promise(resolve => {
      gURLBar.controller.addQueryListener({
        onQueryResults(queryContext) {
          gURLBar.controller.removeQueryListener(this);
          Assert.ok(
            queryContext.heuristicResult,
            "Recieved a heuristic result."
          );
          Assert.equal(
            queryContext.searchString,
            "ex",
            "The heuristic result is based on the correct search string."
          );
          resolve();
        },
      });
    });
    let start = Cu.now();
    EventUtils.sendString("x");
    EventUtils.synthesizeKey("KEY_Enter");
    await recievedResult;
    Assert.ok(Cu.now() - start < TIMEOUT);
  })
);

// The main reason for running each test task in a new tab that's closed when
// the task finishes is to avoid switch-to-tab results.
function taskWithNewTab(fn) {
  return async function () {
    await BrowserTestUtils.withNewTab("about:blank", fn);
  };
}
