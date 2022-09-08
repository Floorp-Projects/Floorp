/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);
var updateService = Cc["@mozilla.org/updates/update-service;1"].getService(
  Ci.nsIApplicationUpdateService
);

add_task(async function test_updates_post_policy() {
  is(
    Services.policies.isAllowed("devtools"),
    false,
    "devtools should be disabled by policy."
  );

  is(
    Services.prefs.getBoolPref("devtools.policy.disabled"),
    true,
    "devtools dedicated disabled pref is set to true"
  );

  Services.prefs.setBoolPref("devtools.policy.disabled", false);

  is(
    Services.prefs.getBoolPref("devtools.policy.disabled"),
    true,
    "devtools dedicated disabled pref can not be updated"
  );

  await testPageBlockedByPolicy("about:devtools-toolbox");
  await testPageBlockedByPolicy("about:debugging");
  await testPageBlockedByPolicy("about:profiling");

  let testURL = "data:text/html;charset=utf-8,test";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testURL,
    false
  );

  let menuButton = document.getElementById("PanelUI-menu-button");
  menuButton.click();
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");
  let moreToolsButtonId = "appMenu-more-button2";
  document.getElementById(moreToolsButtonId).click();
  await BrowserTestUtils.waitForEvent(
    document.getElementById("appmenu-moreTools"),
    "ViewShown"
  );
  is(
    document.getElementById("appmenu-developer-tools-view").children.length,
    2,
    "The developer tools are properly populated"
  );
  window.PanelUI.hide();

  BrowserTestUtils.removeTab(tab);
});

// Copied from ../head.js. head.js was never intended to be used with tests
// that use a JSON file versus calling setupPolicyEngineWithJson so I have
// to copy this function here versus including it.
async function testPageBlockedByPolicy(page, policyJSON) {
  if (policyJSON) {
    await EnterprisePolicyTesting.setupPolicyEngineWithJson(policyJSON);
  }
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      BrowserTestUtils.loadURI(browser, page);
      await BrowserTestUtils.browserLoaded(browser, false, page, true);
      await SpecialPowers.spawn(browser, [page], async function(innerPage) {
        ok(
          content.document.documentURI.startsWith(
            "about:neterror?e=blockedByPolicy"
          ),
          content.document.documentURI +
            " should start with about:neterror?e=blockedByPolicy"
        );
      });
    }
  );
}
