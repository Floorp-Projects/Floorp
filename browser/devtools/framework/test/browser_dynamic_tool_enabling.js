/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that toggling prefs immediately (de)activates the relevant menuitem

let gItemsToTest = {
  "menu_devToolbar": "devtools.toolbar.enabled",
  "menu_browserToolbox": ["devtools.chrome.enabled", "devtools.debugger.remote-enabled", "devtools.debugger.chrome-enabled"],
  "javascriptConsole": "devtools.errorconsole.enabled",
  "menu_devtools_connect": "devtools.debugger.remote-enabled",
};

function expectedAttributeValueFromPrefs(prefs) {
  return prefs.every((pref) => Services.prefs.getBoolPref(pref)) ?
         "" : "true";
}

function checkItem(el, prefs) {
  let expectedValue = expectedAttributeValueFromPrefs(prefs);
  is(el.getAttribute("disabled"), expectedValue, "disabled attribute should match current pref state");
  is(el.getAttribute("hidden"), expectedValue, "hidden attribute should match current pref state");
}

function test() {
  for (let k in gItemsToTest) {
    let el = document.getElementById(k);
    let prefs = gItemsToTest[k];
    if (typeof prefs == "string") {
      prefs = [prefs];
    }
    checkItem(el, prefs);
    for (let pref of prefs) {
      Services.prefs.setBoolPref(pref, !Services.prefs.getBoolPref(pref));
      checkItem(el, prefs);
      Services.prefs.setBoolPref(pref, !Services.prefs.getBoolPref(pref));
      checkItem(el, prefs);
    }
  }
  finish();
}
