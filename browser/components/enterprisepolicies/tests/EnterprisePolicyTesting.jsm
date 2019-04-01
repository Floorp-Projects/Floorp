/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Preferences} = ChromeUtils.import("resource://gre/modules/Preferences.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const {Assert} = ChromeUtils.import("resource://testing-common/Assert.jsm");
ChromeUtils.defineModuleGetter(this, "FileTestUtils",
                               "resource://testing-common/FileTestUtils.jsm");

var EXPORTED_SYMBOLS = ["EnterprisePolicyTesting", "PoliciesPrefTracker"];

var EnterprisePolicyTesting = {
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
      let schemaModule = ChromeUtils.import("resource:///modules/policies/schema.jsm", null);
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
      "browser.policies.runOncePerModification.",
    ];
    for (let base of runOnceBaseKeys) {
      for (let key of Services.prefs.getChildList(base, {})) {
        if (Services.prefs.prefHasUserValue(key))
          Services.prefs.clearUserPref(key);
      }
    }
  },
};

/**
 * This helper will track prefs that have been changed
 * by the policy engine through the setAndLockPref and
 * setDefaultPref APIs (from Policies.jsm) and make sure
 * that they are restored to their original values when
 * the test ends or another test case restarts the engine.
 */
var PoliciesPrefTracker = {
  _originalFunc: null,
  _originalValues: new Map(),

  start() {
    let PoliciesBackstage = ChromeUtils.import("resource:///modules/policies/Policies.jsm", null);
    this._originalFunc = PoliciesBackstage.setDefaultPref;
    PoliciesBackstage.setDefaultPref = this.hoistedSetDefaultPref.bind(this);
  },

  stop() {
    this.restoreDefaultValues();

    let PoliciesBackstage = ChromeUtils.import("resource:///modules/policies/Policies.jsm", null);
    PoliciesBackstage.setDefaultPref = this._originalFunc;
    this._originalFunc = null;
  },

  hoistedSetDefaultPref(prefName, prefValue, locked = false) {
    // If this pref is seen multiple times, the very first
    // value seen is the one that is actually the default.
    if (!this._originalValues.has(prefName)) {
      let defaults = new Preferences({defaultBranch: true});
      let stored = {};

      if (defaults.has(prefName)) {
        stored.originalDefaultValue = defaults.get(prefName);
      } else {
        stored.originalDefaultValue = undefined;
      }

      if (Preferences.isSet(prefName) &&
          Preferences.get(prefName) == prefValue) {
        // If a user value exists, and we're changing the default
        // value to be th same as the user value, that will cause
        // the user value to be dropped. In that case, let's also
        // store it to ensure that we restore everything correctly.
        stored.originalUserValue = Preferences.get(prefName);
      }

      this._originalValues.set(prefName, stored);
    }

    // Now that we've stored the original values, call the
    // original setDefaultPref function.
    this._originalFunc(prefName, prefValue, locked);
  },

  restoreDefaultValues() {
    let defaults = new Preferences({defaultBranch: true});

    for (let [prefName, stored] of this._originalValues) {
      // If a pref was used through setDefaultPref instead
      // of setAndLockPref, it wasn't locked, but calling
      // unlockPref is harmless
      Preferences.unlock(prefName);

      if (stored.originalDefaultValue !== undefined) {
        defaults.set(prefName, stored.originalDefaultValue);
      } else {
        Services.prefs.getDefaultBranch("").deleteBranch(prefName);
      }

      if (stored.originalUserValue !== undefined) {
        Preferences.set(prefName, stored.originalUserValue);
      }
    }

    this._originalValues.clear();
  },
};
