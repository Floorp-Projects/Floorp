const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const CHOICE_PREF = "browser.urlbar.userMadeSearchSuggestionsChoice";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(function* prepare() {
  let engine = yield promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  registerCleanupFunction(function* () {
    Services.search.currentEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF);

    // Disable the notification for future tests so it doesn't interfere with
    // them.  clearUserPref() won't work because by default the pref is false.
    yield setUserMadeChoicePref(true);

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(function* focus() {
  // Focusing the urlbar used to open the popup in order to show the
  // notification, but it doesn't anymore.  Make sure it does not.
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  yield setUserMadeChoicePref(false);
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
});

add_task(function* dismissWithoutResults() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  yield setUserMadeChoicePref(false);
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
  yield setUserMadeChoicePref(false);
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
  yield setUserMadeChoicePref(false);
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
  Assert.ok(!suggestionsPresent());
});

add_task(function* enable() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  yield setUserMadeChoicePref(false);
  gURLBar.blur();
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  assertVisible(true);
  Assert.ok(!suggestionsPresent());
  let enableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-enable"
  );
  let searchPromise = BrowserTestUtils.waitForCondition(suggestionsPresent,
                                                        "waiting for suggestions");
  enableButton.click();
  yield searchPromise;
  // Clicking Yes should trigger a new search so that suggestions appear
  // immediately.
  Assert.ok(suggestionsPresent());
  gURLBar.blur();
  gURLBar.focus();
  // Suggestions should still be present in a new search of course.
  yield promiseAutocompleteResultPopup("bar");
  Assert.ok(suggestionsPresent());
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

add_task(function* multipleWindows() {
  // Opening multiple windows, using their urlbars, and then dismissing the
  // notification in one should dismiss the notification in all.
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  yield setUserMadeChoicePref(false);

  gURLBar.focus();
  yield promiseAutocompleteResultPopup("win1");
  assertVisible(true);

  let win2 = yield BrowserTestUtils.openNewBrowserWindow();
  win2.gURLBar.focus();
  yield promiseAutocompleteResultPopup("win2", win2);
  assertVisible(true, win2);

  let win3 = yield BrowserTestUtils.openNewBrowserWindow();
  win3.gURLBar.focus();
  yield promiseAutocompleteResultPopup("win3", win3);
  assertVisible(true, win3);

  let enableButton = win3.document.getAnonymousElementByAttribute(
    win3.gURLBar.popup, "anonid", "search-suggestions-notification-enable"
  );
  let transitionPromise = promiseTransition(win3);
  enableButton.click();
  yield transitionPromise;
  assertVisible(false, win3);

  win2.gURLBar.focus();
  yield promiseAutocompleteResultPopup("win2done", win2);
  assertVisible(false, win2);

  gURLBar.focus();
  yield promiseAutocompleteResultPopup("win1done");
  assertVisible(false);

  yield BrowserTestUtils.closeWindow(win2);
  yield BrowserTestUtils.closeWindow(win3);
});

add_task(function* enableOutsideNotification() {
  // Setting the suggest.searches pref outside the notification (e.g., by
  // ticking the checkbox in the preferences window) should hide it.
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  yield setUserMadeChoicePref(false);

  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  assertVisible(false);
});

/**
 * Setting the choice pref triggers a pref observer in the urlbar, which hides
 * the notification if it's present.  This function returns a promise that's
 * resolved once the observer fires.
 *
 * @param userMadeChoice  A boolean, the pref's new value.
 * @return A Promise that's resolved when the observer fires -- or, if the pref
 *         is currently the given value, that's resolved immediately.
 */
function setUserMadeChoicePref(userMadeChoice) {
  return new Promise(resolve => {
    let currentUserMadeChoice = Services.prefs.getBoolPref(CHOICE_PREF);
    if (currentUserMadeChoice != userMadeChoice) {
      Services.prefs.addObserver(CHOICE_PREF, function obs(subj, topic, data) {
        Services.prefs.removeObserver(CHOICE_PREF, obs);
        resolve();
      }, false);
    }
    Services.prefs.setBoolPref(CHOICE_PREF, userMadeChoice);
    if (currentUserMadeChoice == userMadeChoice) {
      resolve();
    }
  });
}

function suggestionsPresent() {
  let controller = gURLBar.popup.input.controller;
  let matchCount = controller.matchCount;
  for (let i = 0; i < matchCount; i++) {
    let url = controller.getValueAt(i);
    let mozActionMatch = url.match(/^moz-action:([^,]+),(.*)$/);
    if (mozActionMatch) {
      let [, type, paramStr] = mozActionMatch;
      let params = JSON.parse(paramStr);
      if (type == "searchengine" && "searchSuggestion" in params) {
        return true;
      }
    }
  }
  return false;
}

function assertVisible(visible, win = window) {
  let style =
    win.getComputedStyle(win.gURLBar.popup.searchSuggestionsNotification);
  Assert.equal(style.visibility, visible ? "visible" : "collapse");
}

function promiseTransition(win = window) {
  return new Promise(resolve => {
    win.gURLBar.popup.addEventListener("transitionend", function() {
      // The urlbar needs to handle the transitionend first, but that happens
      // naturally since promises are resolved at the end of the current tick.
      resolve();
    }, {capture: true, once: true});
  });
}
