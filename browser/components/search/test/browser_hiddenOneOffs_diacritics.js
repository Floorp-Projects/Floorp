/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Tests that keyboard navigation in the search panel works as designed.

const searchbar = document.getElementById("searchbar");
const textbox = searchbar._textbox;
const searchPopup = document.getElementById("PopupSearchAutoComplete");
const searchIcon = document.getAnonymousElementByAttribute(searchbar, "anonid",
                                                           "searchbar-search-button");

const diacritic_engine = "Foo \u2661";

var Preferences =
  Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;

add_task(function* init() {
  let currentEngine = Services.search.currentEngine;
  yield promiseNewEngine("testEngine_diacritics.xml", {setAsCurrent: false});
  registerCleanupFunction(() => {
    Services.search.currentEngine = currentEngine;
    Services.prefs.clearUserPref("browser.search.hiddenOneOffs");
  });
});

add_task(function* test_hidden() {
  Preferences.set("browser.search.hiddenOneOffs", diacritic_engine);

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  yield promise;

  ok(!getOneOffs().some(x => x.getAttribute("tooltiptext") == diacritic_engine),
     "Search engines with diacritics are hidden when added to hiddenOneOffs preference.");

  promise = promiseEvent(searchPopup, "popuphidden");
  info("Closing search panel");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promise;
});

add_task(function* test_shown() {
  Preferences.set("browser.search.hiddenOneOffs", "");

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  SimpleTest.executeSoon(() => {
    EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  });
  yield promise;

  ok(getOneOffs().some(x => x.getAttribute("tooltiptext") == diacritic_engine),
     "Search engines with diacritics are shown when removed from hiddenOneOffs preference.");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;
});
