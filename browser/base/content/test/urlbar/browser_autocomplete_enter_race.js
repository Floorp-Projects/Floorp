// The order of these tests matters!

add_task(async function setup() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/?q=%s",
    title: "test"
  });
  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(bm);
  });
  await PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });
  // Needs at least one success.
  ok(true, "Setup complete");
});

add_task(taskWithNewTab(async function test_keyword() {
  await promiseAutocompleteResultPopup("keyword bear");
  gURLBar.focus();
  EventUtils.sendString("d");
  EventUtils.synthesizeKey("KEY_Enter");
  info("wait for the page to load");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                      false, "http://example.com/?q=beard");
}));

add_task(taskWithNewTab(async function test_sametext() {
  await promiseAutocompleteResultPopup("example.com", window, true);

  // Simulate re-entering the same text searched the last time. This may happen
  // through a copy paste, but clipboard handling is not much reliable, so just
  // fire an input event.
  info("synthesize input event");
  let event = document.createEvent("Events");
  event.initEvent("input", true, true);
  gURLBar.dispatchEvent(event);
  EventUtils.synthesizeKey("KEY_Enter");

  info("wait for the page to load");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                       false, "http://example.com/");
}));

add_task(taskWithNewTab(async function test_after_empty_search() {
  await promiseAutocompleteResultPopup("");
  gURLBar.focus();
  gURLBar.value = "e";
  EventUtils.synthesizeKey("x");
  EventUtils.synthesizeKey("KEY_Enter");

  info("wait for the page to load");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                       false, "http://example.com/");
}));

add_task(taskWithNewTab(async function test_disabled_ac() {
  // Disable autocomplete.
  let suggestHistory = Preferences.get("browser.urlbar.suggest.history");
  Preferences.set("browser.urlbar.suggest.history", false);
  let suggestBookmarks = Preferences.get("browser.urlbar.suggest.bookmark");
  Preferences.set("browser.urlbar.suggest.bookmark", false);
  let suggestOpenPages = Preferences.get("browser.urlbar.suggest.openpage");
  Preferences.set("browser.urlbar.suggest.openpages", false);

  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  function cleanup() {
    Preferences.set("browser.urlbar.suggest.history", suggestHistory);
    Preferences.set("browser.urlbar.suggest.bookmark", suggestBookmarks);
    Preferences.set("browser.urlbar.suggest.openpage", suggestOpenPages);

    Services.search.currentEngine = originalEngine;
    let mozSearchEngine = Services.search.getEngineByName("MozSearch");
    if (mozSearchEngine) {
      Services.search.removeEngine(mozSearchEngine);
    }
  }
  registerCleanupFunction(cleanup);

  gURLBar.focus();
  gURLBar.value = "e";
  EventUtils.sendString("x");
  EventUtils.synthesizeKey("KEY_Enter");

  info("wait for the page to load");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                       false, "http://example.com/?q=ex");
  await cleanup();
}));

add_task(taskWithNewTab(async function test_delay() {
  const TIMEOUT = 10000;
  // Set a large delay.
  let delay = Preferences.get("browser.urlbar.delay");
  Preferences.set("browser.urlbar.delay", TIMEOUT);
  registerCleanupFunction(function() {
    Preferences.set("browser.urlbar.delay", delay);
  });

  // This is needed to clear the current value, otherwise autocomplete may think
  // the user removed text from the end.
  let start = Date.now();
  await promiseAutocompleteResultPopup("");
  Assert.ok((Date.now() - start) < TIMEOUT);

  start = Date.now();
  gURLBar.closePopup();
  gURLBar.focus();
  gURLBar.value = "e";
  EventUtils.sendString("x");
  EventUtils.synthesizeKey("KEY_Enter");
  info("wait for the page to load");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedTab.linkedBrowser,
                                       false, "http://example.com/");
  Assert.ok((Date.now() - start) < TIMEOUT);
}));


// The main reason for running each test task in a new tab that's closed when
// the task finishes is to avoid switch-to-tab results.
function taskWithNewTab(fn) {
  return async function() {
    await BrowserTestUtils.withNewTab("about:blank", fn);
  };
}
