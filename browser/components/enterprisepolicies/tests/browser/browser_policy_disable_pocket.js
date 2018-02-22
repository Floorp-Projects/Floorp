/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_POCKET = "extensions.pocket.enabled";

async function checkPocket(shouldBeEnabled) {
  return BrowserTestUtils.waitForCondition(() => {
    return !!PageActions.actionForID("pocket") == shouldBeEnabled;
  }, "Expecting Pocket to be " + shouldBeEnabled);
}

add_task(async function test_disable_firefox_screenshots() {
  await BrowserTestUtils.withNewTab("data:text/html,Test", async function() {

    // Sanity check to make sure Pocket is enabled on tests
    await checkPocket(true);

    await setupPolicyEngineWithJson({
      "policies": {
        "DisablePocket": true
      }
    });

    await checkPocket(false);

    // Clear the change made by the engine to restore Pocket.
    // This is needed in case this test runs twice in a row
    // (such as in test-verify), in order for the first sanity check
    // to pass again.
    await setupPolicyEngineWithJson("");
    Services.prefs.unlockPref(PREF_POCKET);
    Services.prefs.getDefaultBranch("").setBoolPref(PREF_POCKET, true);
  });
});
