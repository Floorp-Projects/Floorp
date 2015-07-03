/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const prefs = {
  "net": [
    "network",
    "netwarn",
    "netxhr",
    "networkinfo"
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
     "log",
     "serviceworkers",
     "sharedworkers",
     "windowlessworkers"
  ]
};

let test = asyncTest(function* () {
  // Set all prefs to true
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      Services.prefs.setBoolPref("devtools.webconsole.filter." + pref, true);
    });
  }

  yield loadTab("about:blank");

  let hud = yield openConsole();

  let hud2 = yield onConsoleOpen(hud);
  let hud3 = yield onConsoleReopen1(hud2);
  yield onConsoleReopen2(hud3);

  // Clear prefs
  for (let category in prefs) {
    prefs[category].forEach(function(pref) {
      Services.prefs.clearUserPref("devtools.webconsole.filter." + pref);
    });
  }
});

function onConsoleOpen(hud) {
  let deferred = promise.defer();

  let hudBox = hud.ui.rootElement;

  // Check if the filters menuitems exists and are checked
  for (let category in prefs) {
    let button = hudBox.querySelector(".webconsole-filter-button[category=\""
                                      + category + "\"]");
    ok(isChecked(button), "main button for " + category +
       " category is checked");

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

  // Re-init the console
  closeConsole().then(() => {
    openConsole().then(deferred.resolve);
  });

  return deferred.promise;
}

function onConsoleReopen1(hud) {
  info("testing after reopening once");
  let deferred = promise.defer();

  let hudBox = hud.ui.rootElement;

  // Check if the filter button and menuitems are unchecked
  for (let category in prefs) {
    let button = hudBox.querySelector(".webconsole-filter-button[category=\""
                                           + category + "\"]");
    ok(isUnchecked(button), "main button for " + category +
       " category is not checked");

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
  closeConsole().then(() => {
    openConsole().then(deferred.resolve);
  });

  return deferred.promise;
}

function onConsoleReopen2(hud) {
  info("testing after reopening again");

  let hudBox = hud.ui.rootElement;

  // Check the main category button is checked and first menuitem is checked
  for (let category in prefs) {
    let button = hudBox.querySelector(".webconsole-filter-button[category=\"" +
                                      category + "\"]");
    ok(isChecked(button), category +
       " button is checked when first pref is true");

    let pref = prefs[category][0];
    let menuitem = hudBox.querySelector("menuitem[prefKey=" + pref + "]");
    ok(isChecked(menuitem), "first " + category + " menuitem is checked");
  }
}

function isChecked(aNode) {
  return aNode.getAttribute("checked") === "true";
}

function isUnchecked(aNode) {
  return aNode.getAttribute("checked") === "false";
}
