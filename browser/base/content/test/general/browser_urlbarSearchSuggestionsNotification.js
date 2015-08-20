const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const CHOICE_PREF = "browser.urlbar.userMadeSearchSuggestionsChoice";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(function* prepare() {
  // The test makes only sense if unified complete is enabled.
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
  let engine = yield promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.urlbar.unifiedcomplete");
    Services.search.currentEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF);

    // Disable the notification for future tests so it doesn't interfere with
    // them.  clearUserPref() won't work because by default the pref is false.
    Services.prefs.setBoolPref(CHOICE_PREF, true);

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(function* focus() {
  // Focusing the urlbar used to open the popup in order to show the
  // notification, but it doesn't anymore.  Make sure it does not.
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
});

add_task(function* dismissWithoutResults() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  let popupPromise = promisePopupShown(gURLBar.popup);
  gURLBar.openPopup();
  yield popupPromise;
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  Assert.equal(gURLBar.popup._matchCount, 0, "popup should have no results");
  let disableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-disable"
  );
  let transitionPromise = promiseTransition();
  disableButton.click();
  yield transitionPromise;
  Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
  yield promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(false);
});

add_task(function* dismissWithResults() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  Assert.ok(gURLBar.popup._matchCount > 0, "popup should have results");
  let disableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-disable"
  );
  let transitionPromise = promiseTransition();
  disableButton.click();
  yield transitionPromise;
  Assert.ok(gURLBar.popup.popupOpen, "popup should remain open");
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
  yield promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(false);
});

add_task(function* disable() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  let disableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-disable"
  );
  let transitionPromise = promiseTransition();
  disableButton.click();
  yield transitionPromise;
  gURLBar.blur();
  yield promiseAutocompleteResultPopup("foo");
  assertSuggestionsPresent(false);
});

add_task(function* enable() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  assertVisible(true);
  assertSuggestionsPresent(false);
  let enableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-enable"
  );
  let searchPromise = promiseSearchComplete();
  enableButton.click();
  yield searchPromise;
  // Clicking Yes should trigger a new search so that suggestions appear
  // immediately.
  assertSuggestionsPresent(true);
  gURLBar.blur();
  gURLBar.focus();
  // Suggestions should still be present in a new search of course.
  yield promiseAutocompleteResultPopup("bar");
  assertSuggestionsPresent(true);
});

add_task(function* privateWindow() {
  // Since suggestions are disabled in private windows, the notification should
  // not appear even when suggestions are otherwise enabled.
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  win.gURLBar.blur();
  win.gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo", win);
  assertVisible(false, win);
  win.gURLBar.blur();
  yield BrowserTestUtils.closeWindow(win);
});

function assertSuggestionsPresent(expectedPresent) {
  let controller = gURLBar.popup.input.controller;
  let matchCount = controller.matchCount;
  let actualPresent = false;
  for (let i = 0; i < matchCount; i++) {
    let url = controller.getValueAt(i);
    let [, type, paramStr] = url.match(/^moz-action:([^,]+),(.*)$/);
    let params = {};
    try {
      params = JSON.parse(paramStr);
    } catch (err) {}
    let isSuggestion = type == "searchengine" && "searchSuggestion" in params;
    actualPresent = actualPresent || isSuggestion;
  }
  Assert.equal(actualPresent, expectedPresent);
}

function assertVisible(visible, win=window) {
  let style =
    win.getComputedStyle(win.gURLBar.popup.searchSuggestionsNotification);
  Assert.equal(style.visibility, visible ? "visible" : "collapse");
}

function promiseTransition() {
  return new Promise(resolve => {
    gURLBar.popup.addEventListener("transitionend", function onEnd() {
      gURLBar.popup.removeEventListener("transitionend", onEnd, true);
      // The urlbar needs to handle the transitionend first, but that happens
      // naturally since promises are resolved at the end of the current tick.
      resolve();
    }, true);
  });
}
