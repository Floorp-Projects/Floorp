/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy_page.html";

async function runTest(privilegedLoad) {
  let prefVals;
  // Test with both pref on and off, unless parent-controlled pref is enabled.
  // This distinction can be removed once SHIP is enabled by default.
  if (
    Services.prefs.getBoolPref("browser.tabs.documentchannel.parent-controlled")
  ) {
    prefVals = [false];
  } else {
    prefVals = [true, false];
  }

  for (let requireUserInteraction of prefVals) {
    Services.prefs.setBoolPref(
      "browser.navigation.requireUserInteraction",
      requireUserInteraction
    );

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TEST_PAGE + "?entry=0"
    );

    assertBackForwardState(false, false);

    await followLink(TEST_PAGE + "?entry=1");

    assertBackForwardState(true, false);

    await followLink(TEST_PAGE + "?entry=2");

    assertBackForwardState(true, false);

    await followLink(TEST_PAGE + "?entry=3");

    assertBackForwardState(true, false);

    // Entry 4 will be added through a user action in browser chrome,
    // giving user interaction to entry 3. Entry 4 should not gain automatic
    // user interaction.
    await privilegedLoad(TEST_PAGE + "?entry=4");

    assertBackForwardState(true, false);

    await followLink(TEST_PAGE + "?entry=5");

    assertBackForwardState(true, false);

    if (!requireUserInteraction) {
      await goBack(TEST_PAGE + "?entry=4");
    }
    await goBack(TEST_PAGE + "?entry=3");

    if (!requireUserInteraction) {
      await goBack(TEST_PAGE + "?entry=2");
      await goBack(TEST_PAGE + "?entry=1");
    }

    assertBackForwardState(true, true);

    await goBack(TEST_PAGE + "?entry=0");

    assertBackForwardState(false, true);

    if (!requireUserInteraction) {
      await goForward(TEST_PAGE + "?entry=1");
      await goForward(TEST_PAGE + "?entry=2");
    }

    await goForward(TEST_PAGE + "?entry=3");

    assertBackForwardState(true, true);

    if (!requireUserInteraction) {
      await goForward(TEST_PAGE + "?entry=4");
    }

    await goForward(TEST_PAGE + "?entry=5");

    assertBackForwardState(true, false);

    BrowserTestUtils.removeTab(tab);
  }

  Services.prefs.clearUserPref("browser.navigation.requireUserInteraction");
}

// Test that we add a user interaction flag to the previous site when loading
// a new site from user interaction with privileged UI, e.g. through the
// URL bar.
add_task(async function test_urlBar() {
  await runTest(async function (url) {
    info(`Loading ${url} via the URL bar.`);
    let browser = gBrowser.selectedBrowser;
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    gURLBar.focus();
    gURLBar.value = url;
    gURLBar.goButton.click();
    await loaded;
  });
});
