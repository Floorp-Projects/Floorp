/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "FileTestUtils",
                                  "resource://testing-common/FileTestUtils.jsm");

async function setupPolicyEngineWithJson(json, customSchema) {
  let filePath;
  if (typeof(json) == "object") {
    filePath = FileTestUtils.getTempFile("policies.json").path;

    // This file gets automatically deleted by FileTestUtils
    // at the end of the test run.
    await OS.File.writeAtomic(filePath, JSON.stringify(json), {
      encoding: "utf-8",
    });
  } else {
    filePath = getTestFilePath(json ? json : "non-existing-file.json");
  }

  Services.prefs.setStringPref("browser.policies.alternatePath", filePath);

  let resolve = null;
  let promise = new Promise((r) => resolve = r);

  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "EnterprisePolicies:AllPoliciesApplied");
    resolve();
  }, "EnterprisePolicies:AllPoliciesApplied");

  // Clear any previously used custom schema
  Cu.unload("resource:///modules/policies/schema.jsm");

  if (customSchema) {
    let schemaModule = ChromeUtils.import("resource:///modules/policies/schema.jsm", {});
    schemaModule.schema = customSchema;
  }

  Services.obs.notifyObservers(null, "EnterprisePolicies:Restart");
  return promise;
}

async function startWithCleanSlate() {
  await setupPolicyEngineWithJson("");
  is(Services.policies.status, Ci.nsIEnterprisePolicies.INACTIVE, "Engine is inactive");
}
