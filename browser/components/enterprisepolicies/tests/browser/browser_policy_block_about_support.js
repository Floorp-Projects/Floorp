/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
                                    "policies": {
                                      "BlockAboutSupport": true,
                                    },
                                  });
});

add_task(async function test_about_support() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:support", false);

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    ok(content.document.documentURI.startsWith("about:neterror?e=blockedByPolicy"),
       content.document.documentURI + "should start with about:neterror?e=blockedByPolicy");

    // There is currently a testing-specific race condition that causes this test
    // to fail, but it is not a problem if we test after the first page load.
    // Until the race condition is fixed, just make sure to test this *after*
    // testing the page load.
    is(Services.policies.isAllowed("about:support"), false,
       "Policy Engine should report about:support as not allowed");
  });

  BrowserTestUtils.removeTab(tab);
});
