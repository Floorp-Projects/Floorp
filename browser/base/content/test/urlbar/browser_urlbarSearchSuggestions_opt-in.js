const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const CHOICE_PREF = "browser.urlbar.userMadeSearchSuggestionsChoice";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(async function prepare() {
  let engine = await promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
  let searchSuggestionsDefault = defaults.getBoolPref("suggest.searches");
  defaults.setBoolPref("suggest.searches", false);
  registerCleanupFunction(async function() {
    defaults.setBoolPref("suggest.searches", searchSuggestionsDefault);
    Services.search.currentEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);

    // Disable the notification for future tests so it doesn't interfere with
    // them.  clearUserPref() won't work because by default the pref is false.
    Services.prefs.setBoolPref(CHOICE_PREF, true);

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(async function focus() {
  // Focusing the urlbar used to open the popup in order to show the
  // notification, but it doesn't anymore.  Make sure it does not.
  setupVisibleNotification();
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
});

add_task(async function dismissWithoutResults() {
  setupVisibleNotification();
  gURLBar.blur();
  gURLBar.focus();
  let popupPromise = promisePopupShown(gURLBar.popup);
  gURLBar.openPopup();
  await popupPromise;
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  Assert.equal(gURLBar.popup._matchCount, 0, "popup should have no results");
  let disableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-disable"
  );
  let transitionPromise = promiseTransition();
  disableButton.click();
  await transitionPromise;
  Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
  await promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(false);
});

add_task(async function dismissWithResults() {
  setupVisibleNotification();
  gURLBar.blur();
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  Assert.ok(gURLBar.popup._matchCount > 0, "popup should have results");
  let disableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-disable"
  );
  let transitionPromise = promiseTransition();
  disableButton.click();
  await transitionPromise;
  Assert.ok(gURLBar.popup.popupOpen, "popup should remain open");
  gURLBar.blur();
  gURLBar.focus();
  Assert.ok(!gURLBar.popup.popupOpen, "popup should remain closed");
  await promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(false);
});

add_task(async function disable() {
  setupVisibleNotification();
  gURLBar.blur();
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  let disableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-disable"
  );
  let transitionPromise = promiseTransition();
  disableButton.click();
  await transitionPromise;
  gURLBar.blur();
  await promiseAutocompleteResultPopup("foo");
  Assert.ok(!suggestionsPresent());
});

add_task(async function enable() {
  setupVisibleNotification();
  gURLBar.blur();
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  assertVisible(true);
  Assert.ok(!suggestionsPresent());
  let enableButton = document.getAnonymousElementByAttribute(
    gURLBar.popup, "anonid", "search-suggestions-notification-enable"
  );
  let searchPromise = BrowserTestUtils.waitForCondition(suggestionsPresent,
                                                        "waiting for suggestions");
  enableButton.click();
  await searchPromise;
  // Clicking Yes should trigger a new search so that suggestions appear
  // immediately.
  Assert.ok(suggestionsPresent());
  gURLBar.blur();
  gURLBar.focus();
  // Suggestions should still be present in a new search of course.
  await promiseAutocompleteResultPopup("bar");
  Assert.ok(suggestionsPresent());
});

add_task(async function privateWindow() {
  // Since suggestions are disabled in private windows, the notification should
  // not appear even when suggestions are otherwise enabled.
  setupVisibleNotification();
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  win.gURLBar.blur();
  win.gURLBar.focus();
  await promiseAutocompleteResultPopup("foo", win);
  assertVisible(false, win);
  win.gURLBar.blur();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function multipleWindows() {
  // Opening multiple windows, using their urlbars, and then dismissing the
  // notification in one should dismiss the notification in all.
  setupVisibleNotification();

  gURLBar.focus();
  await promiseAutocompleteResultPopup("win1");
  assertVisible(true);

  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  win2.gURLBar.focus();
  await promiseAutocompleteResultPopup("win2", win2);
  assertVisible(true, win2);

  let win3 = await BrowserTestUtils.openNewBrowserWindow();
  win3.gURLBar.focus();
  await promiseAutocompleteResultPopup("win3", win3);
  assertVisible(true, win3);

  let enableButton = win3.document.getAnonymousElementByAttribute(
    win3.gURLBar.popup, "anonid", "search-suggestions-notification-enable"
  );
  let transitionPromise = promiseTransition(win3);
  enableButton.click();
  await transitionPromise;
  assertVisible(false, win3);

  win2.gURLBar.focus();
  await promiseAutocompleteResultPopup("win2done", win2);
  assertVisible(false, win2);

  gURLBar.focus();
  await promiseAutocompleteResultPopup("win1done");
  assertVisible(false);

  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(win3);
});

add_task(async function enableOutsideNotification() {
  // Setting the suggest.searches pref outside the notification (e.g., by
  // ticking the checkbox in the preferences window) should hide it.
  setupVisibleNotification();
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  assertVisible(false);
});

function setupVisibleNotification() {
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  // Toggle to reset the whichNotification cache.
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
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
  let check = visible ? "notEqual" : "equal";
  Assert[check](style.display, "none");
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
