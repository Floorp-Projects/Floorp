/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://testing-common/Assert.jsm");
ChromeUtils.defineModuleGetter(this, "FileTestUtils",
                               "resource://testing-common/FileTestUtils.jsm");

this.EXPORTED_SYMBOLS = ["EnterprisePolicyTesting"];

this.EnterprisePolicyTesting = {
  // |json| must be an object representing the desired policy configuration, OR a
  // path to the JSON file containing the policy configuration.
  setupPolicyEngineWithJson: async function setupPolicyEngineWithJson(json, customSchema) {
    let filePath;
    if (typeof(json) == "object") {
      filePath = FileTestUtils.getTempFile("policies.json").path;

      // This file gets automatically deleted by FileTestUtils
      // at the end of the test run.
      await OS.File.writeAtomic(filePath, JSON.stringify(json), {
        encoding: "utf-8",
      });
    } else {
      filePath = json;
    }

    Services.prefs.setStringPref("browser.policies.alternatePath", filePath);

    let promise = new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "EnterprisePolicies:AllPoliciesApplied");
        resolve();
      }, "EnterprisePolicies:AllPoliciesApplied");
    });

    // Clear any previously used custom schema
    Cu.unload("resource:///modules/policies/schema.jsm");

    if (customSchema) {
      let schemaModule = ChromeUtils.import("resource:///modules/policies/schema.jsm", {});
      schemaModule.schema = customSchema;
    }

    Services.obs.notifyObservers(null, "EnterprisePolicies:Restart");
    return promise;
  },

  checkPolicyPref(prefName, expectedValue, expectedLockedness) {
    if (expectedLockedness !== undefined) {
      Assert.equal(Preferences.locked(prefName), expectedLockedness, `Pref ${prefName} is correctly locked/unlocked`);
    }

    Assert.equal(Preferences.get(prefName), expectedValue, `Pref ${prefName} has the correct value`);
  },

  resetRunOnceState: function resetRunOnceState() {
    const runOnceBaseKeys = [
      "browser.policies.runonce.",
      "browser.policies.runOncePerModification."
    ];
    for (let base of runOnceBaseKeys) {
      for (let key of Services.prefs.getChildList(base, {})) {
        if (Services.prefs.prefHasUserValue(key))
          Services.prefs.clearUserPref(key);
      }
    }
  },
};
