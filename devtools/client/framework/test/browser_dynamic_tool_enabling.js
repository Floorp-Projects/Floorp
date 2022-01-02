/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that toggling prefs immediately (de)activates the relevant menuitem

var gItemsToTest = {
  menu_browserToolbox: [
    "devtools.chrome.enabled",
    "devtools.debugger.remote-enabled",
  ],
};

function expectedAttributeValueFromPrefs(prefs) {
  return prefs.every(pref => Services.prefs.getBoolPref(pref)) ? "" : "true";
}

function checkItem(el, prefs) {
  const expectedValue = expectedAttributeValueFromPrefs(prefs);
  is(
    el.getAttribute("disabled"),
    expectedValue,
    "disabled attribute should match current pref state"
  );
  is(
    el.getAttribute("hidden"),
    expectedValue,
    "hidden attribute should match current pref state"
  );
}

function test() {
  for (const k in gItemsToTest) {
    const el = document.getElementById(k);
    const prefs = gItemsToTest[k];
    checkItem(el, prefs);
    for (const pref of prefs) {
      Services.prefs.setBoolPref(pref, !Services.prefs.getBoolPref(pref));
      checkItem(el, prefs);
      Services.prefs.setBoolPref(pref, !Services.prefs.getBoolPref(pref));
      checkItem(el, prefs);
    }
  }
  finish();
}
