/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_notice_in_aboutprefences() {
  await setupPolicyEngineWithJson({
    policies: {
      UserMessaging: {
        MoreFromMozilla: false,
      },
    },
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    let moreFromMozillaCategory = browser.contentDocument.getElementById(
      "category-more-from-mozilla"
    );
    ok(moreFromMozillaCategory.hidden, "The category is hidden");
  });
});
