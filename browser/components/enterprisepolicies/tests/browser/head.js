/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  EnterprisePolicyTesting,
  PoliciesPrefTracker,
} = ChromeUtils.import("resource://testing-common/EnterprisePolicyTesting.jsm", null);
const {TestUtils} = ChromeUtils.import("resource://testing-common/TestUtils.jsm", null);

PoliciesPrefTracker.start();

async function setupPolicyEngineWithJson(json, customSchema) {
  PoliciesPrefTracker.restoreDefaultValues();
  if (typeof(json) != "object") {
    let filePath = getTestFilePath(json ? json : "non-existing-file.json");
    return EnterprisePolicyTesting.setupPolicyEngineWithJson(filePath, customSchema);
  }
  return EnterprisePolicyTesting.setupPolicyEngineWithJson(json, customSchema);
}

function checkLockedPref(prefName, prefValue) {
  EnterprisePolicyTesting.checkPolicyPref(prefName, prefValue, true);
}

function checkUnlockedPref(prefName, prefValue) {
  EnterprisePolicyTesting.checkPolicyPref(prefName, prefValue, false);
}

// Checks that a page was blocked by seeing if it was replaced with about:neterror
async function checkBlockedPage(url, expectedBlocked) {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url,
    waitForLoad: false,
    waitForStateStop: true,
  }, async function() {
    await BrowserTestUtils.waitForCondition(async function() {
      let blocked = await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
        return content.document.documentURI.startsWith("about:neterror");
      });
      return blocked == expectedBlocked;
    }, `Page ${url} block was correct (expected=${expectedBlocked}).`);
  });
}

add_task(async function policies_headjs_startWithCleanSlate() {
  if (Services.policies.status != Ci.nsIEnterprisePolicies.INACTIVE) {
    await setupPolicyEngineWithJson("");
  }
  is(Services.policies.status, Ci.nsIEnterprisePolicies.INACTIVE, "Engine is inactive at the start of the test");
});

registerCleanupFunction(async function policies_headjs_finishWithCleanSlate() {
  if (Services.policies.status != Ci.nsIEnterprisePolicies.INACTIVE) {
    await setupPolicyEngineWithJson("");
  }
  is(Services.policies.status, Ci.nsIEnterprisePolicies.INACTIVE, "Engine is inactive at the end of the test");

  EnterprisePolicyTesting.resetRunOnceState();
  PoliciesPrefTracker.stop();
});

async function testOnAboutAddonsType(type, fn) {
  let useHtmlAboutAddons;
  switch (type) {
    case "XUL":
      useHtmlAboutAddons = false;
      break;
    case "HTML":
      useHtmlAboutAddons = true;
      break;
    default:
      throw new Error(`Unknown about:addons type ${type}`);
  }
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", useHtmlAboutAddons]],
  });
  info(`Run tests on ${type} about:addons`);
  await fn();
  await SpecialPowers.popPrefEnv();
}


