const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const CHOICE_PREF = "browser.urlbar.userMadeSearchSuggestionsChoice";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(function* prepare() {
  let engine = yield promiseNewEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  registerCleanupFunction(function () {
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

add_task(function* focus_allSuggestionsDisabled() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, false);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  yield promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(false);
});

add_task(function* focus_noChoiceMade() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  gURLBar.blur();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  gURLBar.focus();
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open again");
  assertVisible(true);
});

add_task(function* dismissWithoutResults() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  gURLBar.blur();
  gURLBar.focus();
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
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  yield promiseAutocompleteResultPopup("foo");
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

function assertVisible(visible) {
  let style =
    window.getComputedStyle(gURLBar.popup.searchSuggestionsNotification);
  Assert.equal(style.visibility, visible ? "visible" : "collapse");
}

function promiseNewEngine(basename) {
  return new Promise((resolve, reject) => {
    info("Waiting for engine to be added: " + basename);
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, Ci.nsISearchEngine.TYPE_MOZSEARCH, "",
                              false, {
      onSuccess: function (engine) {
        info("Search engine added: " + basename);
        registerCleanupFunction(() => Services.search.removeEngine(engine));
        resolve(engine);
      },
      onError: function (errCode) {
        Assert.ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });
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
