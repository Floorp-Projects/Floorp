/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_notice_in_aboutprefences() {
  await setupPolicyEngineWithJson({
    policies: {},
  });

  await BrowserTestUtils.withNewTab("about:preferences", async browser => {
    ok(
      !browser.contentDocument.getElementById("policies-container").hidden,
      "The Policies notice was made visible in about:preferences"
    );
  });
});
