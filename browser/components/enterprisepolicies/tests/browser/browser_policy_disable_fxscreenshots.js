/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_DISABLE_FX_SCREENSHOTS = "extensions.screenshots.disabled";

async function checkScreenshots(shouldBeEnabled) {
  return BrowserTestUtils.waitForCondition(() => {
    return !!PageActions.actionForID("screenshots") == shouldBeEnabled;
  }, "Expecting screenshots to be " + shouldBeEnabled);
}

add_task(async function test_disable_firefox_screenshots() {
  await BrowserTestUtils.withNewTab("data:text/html,Test", async function() {
    await setupPolicyEngineWithJson("");
    is(Services.policies.status, Services.policies.INACTIVE, "Start with no policies");

    // Firefox Screenshots are disabled in tests, so make sure we enable
    // it first to ensure that the test is valid.
    Services.prefs.setBoolPref(PREF_DISABLE_FX_SCREENSHOTS, false);
    await checkScreenshots(true);

    await setupPolicyEngineWithJson({
      "policies": {
        "DisableFirefoxScreenshots": true
      }
    });

    is(Services.policies.status, Services.policies.ACTIVE, "Policy engine is active");
    await checkScreenshots(false);

    // Clear the change we made and make sure it remains disabled.
    await setupPolicyEngineWithJson("");

    Services.prefs.unlockPref(PREF_DISABLE_FX_SCREENSHOTS);
    Services.prefs.clearUserPref(PREF_DISABLE_FX_SCREENSHOTS);

    await checkScreenshots(false);
  });
});
