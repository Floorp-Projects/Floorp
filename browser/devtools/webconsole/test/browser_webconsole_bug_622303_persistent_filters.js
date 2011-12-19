/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let prefService = Services.prefs;

  let prefs = {
    "net": [
      "network",
      "networkinfo"
    ],
    "css": [
      "csserror",
      "cssparser"
    ],
    "js": [
      "exception",
      "jswarn"
    ],
    "logging": [
       "error",
       "warn",
       "info",
       "log"
    ]
  };

  // Set all prefs to true
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      prefService.setBoolPref("devtools.webconsole.filter." + pref, true);
    });
  }

  addTab("about:blank");
  openConsole();

  let hud = HUDService.getHudByWindow(content);

  // Check if the filters menuitems exists and are checked
  for (let category in prefs) {
    let button = hud.HUDBox.querySelector(".webconsole-filter-button[category=\""
                                           + category + "\"]");
    ok(isChecked(button), "main button for " + category + " category is checked");

    prefs[category].forEach(function(pref) {
      let menuitem = hud.HUDBox.querySelector("menuitem[prefKey=" + pref + "]");
      ok(isChecked(menuitem), "menuitem for " + pref + " is checked");
    });
  }

  // Set all prefs to false
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      HUDService.setFilterState(hud.hudId, pref, false);
    });
  }

  //Re-init the console
  closeConsole();
  openConsole();

  hud = HUDService.getHudByWindow(content);

  // Check if the filter button and menuitems are unchecked
  for (let category in prefs) {
    let button = hud.HUDBox.querySelector(".webconsole-filter-button[category=\""
                                           + category + "\"]");
    ok(isUnchecked(button), "main button for " + category + " category is not checked");

    prefs[category].forEach(function(pref) {
      let menuitem = hud.HUDBox.querySelector("menuitem[prefKey=" + pref + "]");
      ok(isUnchecked(menuitem), "menuitem for " + pref + " is not checked");
    });
  }

  // Set first pref in each category to true
  for (let category in prefs) {
    HUDService.setFilterState(hud.hudId, prefs[category][0], true);
  }

  // Re-init the console
  closeConsole();
  openConsole();

  hud = HUDService.getHudByWindow(content);

  // Check the main category button is checked and first menuitem is checked
  for (let category in prefs) {
    let button = hud.HUDBox.querySelector(".webconsole-filter-button[category=\""
                                           + category + "\"]");
    ok(isChecked(button), category  + " button is checked when first pref is true");

    let pref = prefs[category][0];
    let menuitem = hud.HUDBox.querySelector("menuitem[prefKey=" + pref + "]");
    ok(isChecked(menuitem), "first " + category + " menuitem is checked");
  }

  // Clear prefs
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      prefService.clearUserPref("devtools.webconsole.filter." + pref);
    });
  }

  gBrowser.removeCurrentTab();
  finish();
}

function isChecked(aNode) {
  return aNode.getAttribute("checked") === "true";
}

function isUnchecked(aNode) {
  return aNode.getAttribute("checked") === "false";
}
