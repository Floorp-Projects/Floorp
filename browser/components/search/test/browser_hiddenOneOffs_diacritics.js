/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Tests that keyboard navigation in the search panel works as designed.

const searchbar = document.getElementById("searchbar");
const textbox = searchbar._textbox;
const searchPopup = document.getElementById("PopupSearchAutoComplete");

const diacritic_engine = "Foo \u2661";

let ns = {};
Cu.import("resource://gre/modules/Preferences.jsm", ns);

// Get an array of the one-off buttons.
function getOneOffs() {
  let oneOffs = [];
  let oneOff =
    document.getAnonymousElementByAttribute(searchPopup, "anonid",
                                            "search-panel-one-offs");
  for (oneOff = oneOff.firstChild; oneOff; oneOff = oneOff.nextSibling) {
    if (oneOff.classList.contains("dummy"))
      break;
    oneOffs.push(oneOff);
  }

  return oneOffs;
}

add_task(function* init() {
  const cachedPref = Services.prefs.getCharPref("browser.search.hiddenOneOffs");

  let currentEngine = Services.search.currentEngine;

  yield promiseNewEngine("testEngine_diacritics.xml", false);

  // Add testEngine and use it as the current search service so that the diacritic
  // engine will be in the shownOneOffs list and so that the search bar won't
  // attempt to call a remote search service during testing.
  yield promiseNewEngine("testEngine.xml", false);
  Services.search.currentEngine = Services.search.getEngineByName("Foo");
  textbox.value = "Foo";
  registerCleanupFunction(() => {
    textbox.value = "";
    Services.search.currentEngine = currentEngine;
    ns.Preferences.set("browser.search.hiddenOneOffs", cachedPref);
  });
});

add_task(function* test_hidden() {
  ns.Preferences.set("browser.search.hiddenOneOffs", diacritic_engine);

  let promise = promiseEvent(searchPopup, "popupshown");
  searchbar.focus();
  yield promise;

  ok(!getOneOffs().some(x => x.getAttribute("label") == diacritic_engine),
     "Search engines with diacritics are hidden when added to hiddenOneOffs preference.");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;
  gURLBar.focus();
});

add_task(function* test_shown() {
  ns.Preferences.set("browser.search.hiddenOneOffs", "");

  let promise = promiseEvent(searchPopup, "popupshown");
  searchbar.focus();
  yield promise;

  ok(getOneOffs().some(x => x.getAttribute("label") == diacritic_engine),
     "Search engines with diacritics are shown when removed from hiddenOneOffs preference.");

  promise = promiseEvent(searchPopup, "popuphidden");
  searchPopup.hidePopup();
  yield promise;
});
