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
  // Dynamically toggling the PREF_DISABLE_FX_SCREENSHOTS is very finicky, because
  // that pref is being watched, and it makes the Firefox Screenshots system add-on
  // to start or stop, causing intermittency.
  //
  // Firefox Screenshots is disabled by default on tests (in
  // testing/profiles/common/user.js). What we do here to test this policy is to enable
  // it on this specific test folder (through browser.ini) and then we let the policy
  // engine be responsible for disabling Firefox Screenshots in this case.

  is(Services.prefs.getBoolPref(PREF_DISABLE_FX_SCREENSHOTS), true, "Screenshots pref is disabled");

  await BrowserTestUtils.withNewTab("data:text/html,Test", async function() {
    await checkScreenshots(false);
  });
});
