/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SettingsDB.jsm");

SimpleTest.waitForExplicitFinish();

let settings = {
  "setting1": "value1"
};

let settingsDB;

const saveTestSettingsJson = () => {
  let file = FileUtils.getFile("ProfD", ["settings.json"], false);
  let foStream = FileUtils.openFileOutputStream(file);
  let json = JSON.stringify(settings);
  foStream.write(json, json.length);
  foStream.close();
};

const getSettings = () => {
  return new Promise((resolve, reject) => {
    settingsDB.newTxn("readonly", SETTINGSSTORE_NAME, (txn, store) => {
      txn.onabort = reject;
      txn.onerror = reject;
      let req = store.getAll();
      req.onsuccess = () => {
        resolve(req.result);
      };
      req.onerror = reject;
    }, null, reject);
  });
};

const checkSettings = expected => {
  return getSettings().then(settings => {
    settings.forEach(setting => {
      if (expected.has(setting.settingName)) {
        let value = expected.get(setting.settingName);
        ok(value == setting.defaultValue,
           `${setting.defaultValue} should be ${value}`);
        expected.delete(setting.settingName);
      }
    });
    ok(expected.size == 0, `should remove all expected settings`);
  });
};

const setFirstRun = () => {
  let promises = [];
  promises.push(new Promise(resolve => {
    SpecialPowers.pushPrefEnv({
      "set": [["gecko.mstone", ""]]
    }, resolve);
  }));
  promises.push(new Promise(resolve => {
    SpecialPowers.pushPrefEnv({
      "set": [["gecko.buildID", ""]]
    }, resolve);
  }));

  return Promise.all(promises);
};

const cleanup = () => {
  settingsDB.close();
  settingsDB = null;
  next();
};

const tests = [() => {
  // Create dummy settings.json in the profile dir.
  saveTestSettingsJson();
  ok(true, "should create settings.json");
  next();
}, () => {
  // settings.json should be applied on first boot.
  settingsDB = new SettingsDB();
  settingsDB.init();
  let expected = new Map();
  expected.set("setting1", "value1");
  checkSettings(expected).then(next);
}, () => {
  cleanup();
}, () => {
  // Modifying settings.json but not updating the platform should not
  // apply the changes to settings.json.
  settings["setting1"] = "modifiedValue1";
  settings["setting2"] = "value2";
  saveTestSettingsJson();
  settingsDB = new SettingsDB();
  settingsDB.init();
  let expected = new Map();
  expected.set("setting1", "value1");
  checkSettings(expected).then(next);
}, () => {
  cleanup();
}, () => {
  setFirstRun().then(next);
}, () => {
  // Updating the platform should apply changes to settings.json.
  settingsDB = new SettingsDB();
  settingsDB.init();
  let expected = new Map();
  expected.set("setting1", "modifiedValue1");
  expected.set("setting2", "value2");
  checkSettings(expected).then(next);
}, () => {
  cleanup();
}, () => {
  setFirstRun().then(next);
}, () => {
  // Updating the platform and bumping DB version should also apply changes
  // to settings.json
  settings["setting2"] = "modifiedValue2";
  saveTestSettingsJson();
  settingsDB = new SettingsDB();
  SettingsDB.dbVersion = SETTINGSDB_VERSION + 1;
  settingsDB.init();
  let expected = new Map();
  expected.set("setting1", "modifiedValue1");
  expected.set("setting2", "modifiedValue2");
  checkSettings(expected).then(next);
}, () => {
  cleanup();
}];

const next = () => {
  let step = tests.shift();
  if (!step) {
    return SimpleTest.finish();
  }
  try {
    step();
  } catch(e) {
    ok(false, "Test threw: " + e);
  }
}

SpecialPowers.pushPrefEnv({"set":[["settings.api.db.version", 0]]}, next);
