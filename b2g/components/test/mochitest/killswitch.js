/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function setupSettings(target) {
  ok((Object.keys(initialSettingsValues).length > 0), "Has at least one setting");

  Object.keys(initialSettingsValues).forEach(k => {
    ok(Object.keys(target).indexOf(k) !== -1, "Same settings set");
  });

  var lock = navigator.mozSettings.createLock();
  lock.set(initialSettingsValues);
}

function testSettingsInitial(next) {
  var promises = [];
  for (var setting in initialSettingsValues) {
    promises.push(navigator.mozSettings.createLock().get(setting));
  }

  Promise.all(promises).then(values => {
    values.forEach(set => {
      var key = Object.keys(set)[0];
      var value = set[key];
      is(value, initialSettingsValues[key], "Value of " + key + " is initial one");
    });
    next();
  });
}

function testSettingsExpected(target, next) {
  var promises = [];
  for (var setting in initialSettingsValues) {
    promises.push(navigator.mozSettings.createLock().get(setting));
  }

  Promise.all(promises).then(values => {
    values.forEach(set => {
      var key = Object.keys(set)[0];
      var value = set[key];
      is(value, target[key], "Value of " + key + " is expected one");
    });
    next();
  });
}

function testSetPrefValue(prefName, prefValue) {
  switch (typeof prefValue) {
    case "boolean":
      SpecialPowers.setBoolPref(prefName, prefValue);
      break;

    case "number":
      SpecialPowers.setIntPref(prefName, prefValue);
      break;

    case "string":
      SpecialPowers.setCharPref(prefName, prefValue);
      break;

    default:
      is(false, "Unexpected pref type");
      break;
  }
}

function testGetPrefValue(prefName, prefValue) {
  var rv = undefined;

  switch (typeof prefValue) {
    case "boolean":
      rv = SpecialPowers.getBoolPref(prefName);
      break;

    case "number":
      rv = SpecialPowers.getIntPref(prefName);
      break;

    case "string":
      rv = SpecialPowers.getCharPref(prefName);
      break;

    default:
      is(false, "Unexpected pref type");
      break;
  }

  return rv;
}

function setupPrefs(target) {
  ok((Object.keys(initialPrefsValues).length > 0), "Has at least one pref");

  Object.keys(initialPrefsValues).forEach(k => {
    ok(Object.keys(target).indexOf(k) !== -1, "Same pref set");
  });

  Object.keys(initialPrefsValues).forEach(key => {
    testSetPrefValue(key, initialPrefsValues[key]);
  });
}

function testPrefsInitial() {
  Object.keys(initialPrefsValues).forEach(key => {
    var value = testGetPrefValue(key, initialPrefsValues[key]);
    is(value, initialPrefsValues[key], "Value of " + key + " is initial one");
  });
}

function testPrefsExpected(target) {
  Object.keys(target).forEach(key => {
    var value = testGetPrefValue(key, target[key]);
    is(value, target[key], "Value of " + key + " is initial one");
  });
}

function finish() {
  SpecialPowers.removePermission("killswitch", document);
  SimpleTest.finish();
}

function addPermissions() {
  if (SpecialPowers.hasPermission("killswitch", document)) {
    startTests();
  } else {
    var allow = SpecialPowers.Ci.nsIPermissionManager.ALLOW_ACTION;
    [ "killswitch", "settings-api-read", "settings-api-write",
      "settings-read", "settings-write", "settings-clear"
    ].forEach(perm => {
      SpecialPowers.addPermission(perm, allow, document);
    });
    window.location.reload();
  }
}

function loadSettings() {
  var url = SimpleTest.getTestFileURL("file_loadserver.js");
  var script = SpecialPowers.loadChromeScript(url);
}

function addPrefs() {
  SpecialPowers.pushPrefEnv({"set": [
    ["dom.ignore_webidl_scope_checks", true],
    ["dom.mozKillSwitch.enabled", true],
  ]}, addPermissions);
}
