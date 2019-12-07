/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const policiesToTest = {
  BlockAboutAddons: "about:addons",
  BlockAboutConfig: "about:config",
  BlockAboutProfiles: "about:profiles",
  BlockAboutSupport: "about:support",
};

add_task(async function testAboutTask() {
  for (let policy in policiesToTest) {
    let policyJSON = { policies: {} };
    policyJSON.policies[policy] = true;
    await testPageBlockedByPolicy(policyJSON, policiesToTest[policy]);
  }
  let policyJSON = { policies: {} };
  policyJSON.policies.PasswordManagerEnabled = false;
  await testPageBlockedByPolicy(policyJSON, "about:logins");
});

async function testPageBlockedByPolicy(policyJSON, page) {
  await setupPolicyEngineWithJson(policyJSON);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      BrowserTestUtils.loadURI(browser, page);
      await BrowserTestUtils.browserLoaded(browser, false, page, true);
      await ContentTask.spawn(browser, page, async function(innerPage) {
        ok(
          content.document.documentURI.startsWith(
            "about:neterror?e=blockedByPolicy"
          ),
          content.document.documentURI +
            " should start with about:neterror?e=blockedByPolicy"
        );

        // There is currently a testing-specific race condition that causes this test
        // to fail, but it is not a problem if we test after the first page load.
        // Until the race condition is fixed, just make sure to test this *after*
        // testing the page load.
        is(
          Services.policies.isAllowed(innerPage),
          false,
          `Policy Engine should report ${innerPage} as not allowed`
        );
      });
    }
  );
}
