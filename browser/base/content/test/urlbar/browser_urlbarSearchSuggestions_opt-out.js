// The order of the tests here matters!

const SUGGEST_ALL_PREF = "browser.search.suggest.enabled";
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const CHOICE_PREF = "browser.urlbar.userMadeSearchSuggestionsChoice";
const TIMES_PREF = "browser.urlbar.timesBeforeHidingSuggestionsHint";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const ONEOFF_PREF = "browser.urlbar.oneOffSearches";

add_task(async function prepare() {
  let engine = await promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
  let searchSuggestionsDefault = defaults.getBoolPref("suggest.searches");
  defaults.setBoolPref("suggest.searches", true);
  let suggestionsChoice = Services.prefs.getBoolPref(CHOICE_PREF);
  Services.prefs.setBoolPref(CHOICE_PREF, false);
  let oneOffs = Services.prefs.getBoolPref(ONEOFF_PREF);
  Services.prefs.setBoolPref(ONEOFF_PREF, true);
  registerCleanupFunction(async function() {
    defaults.setBoolPref("suggest.searches", searchSuggestionsDefault);
    Services.search.currentEngine = oldCurrentEngine;
    Services.prefs.clearUserPref(SUGGEST_ALL_PREF);
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    Services.prefs.setBoolPref(CHOICE_PREF, suggestionsChoice);
    Services.prefs.setBoolPref(ONEOFF_PREF, oneOffs);
    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(async function focus() {
  // Focusing the urlbar should open the popup in order to show the
  // notification.
  setupVisibleHint();
  gURLBar.blur();
  let popupPromise = promisePopupShown(gURLBar.popup);
  gURLBar.focus();
  await popupPromise;
  Assert.ok(gURLBar.popup.popupOpen, "popup should be open");
  assertVisible(true);
  assertFooterVisible(false);
  Assert.equal(gURLBar.popup._matchCount, 0, "popup should have no results");

  // Start searching.
  EventUtils.synthesizeKey("r", {});
  EventUtils.synthesizeKey("n", {});
  EventUtils.synthesizeKey("d", {});
  await promiseSearchComplete();
  Assert.ok(suggestionsPresent());
  assertVisible(true);
  assertFooterVisible(true);

  // Check the Change Options link.
  let changeOptionsLink = document.getElementById("search-suggestions-change-settings");
  let prefsPromise = BrowserTestUtils.waitForLocationChange(gBrowser, "about:preferences#general-search");
  changeOptionsLink.click();
  await prefsPromise;
  Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
});


add_task(async function privateWindow() {
  // Since suggestions are disabled in private windows, the notification should
  // not appear even when suggestions are otherwise enabled.
  setupVisibleHint();
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await promiseAutocompleteResultPopup("foo", win);
  assertVisible(false, win);
  assertFooterVisible(true, win);
  win.gURLBar.blur();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function enableOutsideNotification() {
  // Setting the suggest.searches pref outside the notification (e.g., by
  // ticking the checkbox in the preferences window) should hide it.
  setupVisibleHint();
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  await promiseAutocompleteResultPopup("foo");
  assertVisible(false);
  assertFooterVisible(true);
});

add_task(async function userMadeChoice() {
  // If the user made a choice already, he should not see the hint.
  setupVisibleHint();
  Services.prefs.setBoolPref(CHOICE_PREF, true);
  await promiseAutocompleteResultPopup("foo");
  assertVisible(false);
  assertFooterVisible(true);
});

function setupVisibleHint() {
  Services.prefs.clearUserPref(TIMES_PREF);
  Services.prefs.setBoolPref(SUGGEST_ALL_PREF, true);
  // Toggle to reset the whichNotification cache.
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, false);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
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
function assertFooterVisible(visible, win = window) {
  let style = win.getComputedStyle(win.gURLBar.popup.footer);
  Assert.equal(style.visibility, visible ? "visible" : "collapse");
}
