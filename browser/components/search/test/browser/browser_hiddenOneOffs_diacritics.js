/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Tests that keyboard navigation in the search panel works as designed.

const searchPopup = document.getElementById("PopupSearchAutoComplete");

const diacritic_engine = "Foo \u2661";

var Preferences = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm",
  {}
).Preferences;

let searchIcon;

add_task(async function init() {
  let searchbar = await gCUITestUtils.addSearchBar();
  registerCleanupFunction(() => {
    gCUITestUtils.removeSearchBar();
  });
  searchIcon = searchbar.querySelector(".searchbar-search-button");

  let defaultEngine = await Services.search.getDefault();
  await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "testEngine_diacritics.xml"
  );
  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
    Services.prefs.clearUserPref("browser.search.hiddenOneOffs");
  });
});

add_task(async function test_hidden() {
  Preferences.set("browser.search.hiddenOneOffs", diacritic_engine);

  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await promise;

  ok(
    !getOneOffs().some(x => x.getAttribute("tooltiptext") == diacritic_engine),
    "Search engines with diacritics are hidden when added to hiddenOneOffs preference."
  );

  promise = promiseEvent(searchPopup, "popuphidden");
  info("Closing search panel");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;
});

add_task(async function test_shown() {
  Preferences.set("browser.search.hiddenOneOffs", "");

  let oneOffsContainer = searchPopup.searchOneOffsContainer;
  let shownPromise = promiseEvent(searchPopup, "popupshown");
  let builtPromise = promiseEvent(oneOffsContainer, "rebuild");
  info("Opening search panel");

  EventUtils.synthesizeMouseAtCenter(searchIcon, {});
  await Promise.all([shownPromise, builtPromise]);

  ok(
    getOneOffs().some(x => x.getAttribute("tooltiptext") == diacritic_engine),
    "Search engines with diacritics are shown when removed from hiddenOneOffs preference."
  );

  let promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  await promise;
});
