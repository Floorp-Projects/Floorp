/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_page.html";

// Regression test for navigating back after visiting an about: page
// loaded in the parent process.
add_task(async function test_about_back() {
  // Test with both pref on and off
  for (let requireUserInteraction of [true, false]) {
    Services.prefs.setBoolPref(
      "browser.navigation.requireUserInteraction",
      requireUserInteraction
    );

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PAGE + "?entry=0"
    );
    let browser = tab.linkedBrowser;
    assertBackForwardState(false, false);

    await followLink(TEST_PAGE + "?entry=1");
    assertBackForwardState(true, false);

    await followLink(TEST_PAGE + "?entry=2");
    assertBackForwardState(true, false);

    // Add some user interaction to entry 2
    await BrowserTestUtils.synthesizeMouse("body", 0, 0, {}, browser, true);

    await loadURI("about:config");
    assertBackForwardState(true, false);

    await goBack(TEST_PAGE + "?entry=2");
    assertBackForwardState(true, true);

    if (!requireUserInteraction) {
      await goBack(TEST_PAGE + "?entry=1");
      assertBackForwardState(true, true);
    }

    await goBack(TEST_PAGE + "?entry=0");
    assertBackForwardState(false, true);

    if (!requireUserInteraction) {
      await goForward(TEST_PAGE + "?entry=1");
      assertBackForwardState(true, true);
    }

    await goForward(TEST_PAGE + "?entry=2");
    assertBackForwardState(true, true);

    await goForward("about:config");
    assertBackForwardState(true, false);

    BrowserTestUtils.removeTab(tab);
  }

  Services.prefs.clearUserPref("browser.navigation.requireUserInteraction");
});
