/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

function runTests() {
  // turn off preload to ensure that a newtab page loads as disabled
  Services.prefs.setBoolPref(PRELOAD_PREF, false);
  Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, false);
  yield addNewTabPageTab();

  let search = getContentDocument().getElementById("newtab-search-form");
  is(search.style.width, "", "search form has no width yet");

  NewTabUtils.allPages.enabled = true;
  isnot(search.style.width, "", "search form has width set");

  // restore original state
  Services.prefs.clearUserPref(PRELOAD_PREF);
}
