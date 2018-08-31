/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function test_about_config() {
  await setupPolicyEngineWithJson({
                                    "policies": {
                                      "BlockAboutConfig": true,
                                    },
                                  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:config", false);

  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    ok(content.document.documentURI.startsWith("about:neterror"),
       "about:config should display the net error page");

    // There is currently a testing-specific race condition that causes this test
    // to fail, but it is not a problem if we test after the first page load.
    // Until the race condition is fixed, just make sure to test this *after*
    // testing the page load.
    is(Services.policies.isAllowed("about:config"), false,
       "Policy Engine should report about:config as not allowed");
  });

  BrowserTestUtils.removeTab(tab);
});
