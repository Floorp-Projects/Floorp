/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { updateAppInfo, getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
const { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);
const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

updateAppInfo({
  name: "XPCShell",
  ID: "xpcshell@tests.mozilla.org",
  version: "48",
  platformVersion: "48",
});

// This initializes the policy engine for xpcshell tests
let policies = Cc["@mozilla.org/enterprisepolicies;1"].getService(
  Ci.nsIObserver
);
policies.observe(null, "policies-startup", null);

async function setupPolicyEngineWithJson(json, customSchema) {
  if (typeof json != "object") {
    let filePath = do_get_file(json ? json : "non-existing-file.json").path;
    return EnterprisePolicyTesting.setupPolicyEngineWithJson(
      filePath,
      customSchema
    );
  }
  return EnterprisePolicyTesting.setupPolicyEngineWithJson(json, customSchema);
}

function checkLockedPref(prefName, prefValue) {
  equal(
    Preferences.locked(prefName),
    true,
    `Pref ${prefName} is correctly locked`
  );
  equal(
    Preferences.get(prefName),
    prefValue,
    `Pref ${prefName} has the correct value`
  );
}

function checkUnlockedPref(prefName, prefValue) {
  equal(
    Preferences.locked(prefName),
    false,
    `Pref ${prefName} is correctly unlocked`
  );
  equal(
    Preferences.get(prefName),
    prefValue,
    `Pref ${prefName} has the correct value`
  );
}

function checkUserPref(prefName, prefValue) {
  equal(
    Preferences.get(prefName),
    prefValue,
    `Pref ${prefName} has the correct value`
  );
}

function checkClearPref(prefName, prefValue) {
  equal(
    Services.prefs.prefHasUserValue(prefName),
    false,
    `Pref ${prefName} has no user value`
  );
}

function checkDefaultPref(prefName, prefValue) {
  let defaultPrefBranch = Services.prefs.getDefaultBranch("");
  let prefType = defaultPrefBranch.getPrefType(prefName);
  notEqual(
    prefType,
    Services.prefs.PREF_INVALID,
    `Pref ${prefName} is set on the default branch`
  );
}

function checkUnsetPref(prefName) {
  let defaultPrefBranch = Services.prefs.getDefaultBranch("");
  let prefType = defaultPrefBranch.getPrefType(prefName);
  equal(
    prefType,
    Services.prefs.PREF_INVALID,
    `Pref ${prefName} is not set on the default branch`
  );
}
