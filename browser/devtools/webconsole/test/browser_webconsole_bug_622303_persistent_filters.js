/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let prefs = {
  "net": [
    "network",
    "netwarn",
    "networkinfo",
  ],
  "css": [
    "csserror",
    "cssparser",
    "csslog"
  ],
  "js": [
    "exception",
    "jswarn",
    "jslog",
  ],
  "logging": [
     "error",
     "warn",
     "info",
     "log"
  ]
};

function test() {
  // Set all prefs to true
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      Services.prefs.setBoolPref("devtools.webconsole.filter." + pref, true);
    });
  }

  addTab("about:blank");
  openConsole(null, onConsoleOpen);
}

function onConsoleOpen(hud) {
  let hudBox = hud.ui.rootElement;

  // Check if the filters menuitems exists and are checked
  for (let category in prefs) {
    let button = hudBox.querySelector(".webconsole-filter-button[category=\""
                                      + category + "\"]");
    ok(isChecked(button), "main button for " + category + " category is checked");

    prefs[category].forEach(function(pref) {
      let menuitem = hudBox.querySelector("menuitem[prefKey=" + pref + "]");
      ok(isChecked(menuitem), "menuitem for " + pref + " is checked");
    });
  }

  // Set all prefs to false
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      hud.setFilterState(pref, false);
    });
  }

  //Re-init the console
  closeConsole(null, function() {
    openConsole(null, onConsoleReopen1);
  });
}

function onConsoleReopen1(hud) {
  let hudBox = hud.ui.rootElement;

  // Check if the filter button and menuitems are unchecked
  for (let category in prefs) {
    let button = hudBox.querySelector(".webconsole-filter-button[category=\""
                                           + category + "\"]");
    ok(isUnchecked(button), "main button for " + category + " category is not checked");

    prefs[category].forEach(function(pref) {
      let menuitem = hudBox.querySelector("menuitem[prefKey=" + pref + "]");
      ok(isUnchecked(menuitem), "menuitem for " + pref + " is not checked");
    });
  }

  // Set first pref in each category to true
  for (let category in prefs) {
    hud.setFilterState(prefs[category][0], true);
  }

  // Re-init the console
  closeConsole(null, function() {
    openConsole(null, onConsoleReopen2);
  });
}

function onConsoleReopen2(hud) {
  let hudBox = hud.ui.rootElement;

  // Check the main category button is checked and first menuitem is checked
  for (let category in prefs) {
    let button = hudBox.querySelector(".webconsole-filter-button[category=\""
                                           + category + "\"]");
    ok(isChecked(button), category  + " button is checked when first pref is true");

    let pref = prefs[category][0];
    let menuitem = hudBox.querySelector("menuitem[prefKey=" + pref + "]");
    ok(isChecked(menuitem), "first " + category + " menuitem is checked");
  }

  // Clear prefs
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      Services.prefs.clearUserPref("devtools.webconsole.filter." + pref);
    });
  }

  prefs = null;
  finishTest();
}

function isChecked(aNode) {
  return aNode.getAttribute("checked") === "true";
}

function isUnchecked(aNode) {
  return aNode.getAttribute("checked") === "false";
}
