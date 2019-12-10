/*
 * This file contains tests for the Preferences search bar.
 */

// Enabling Searching functionatily. Will display search bar form this testcase forward.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.search", true]],
  });
});

/**
 * Test for searching for the "Set Home Page" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });

  // Set custom URL so bookmark button will be shown on the page (otherwise it is hidden)
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.startup.homepage", "about:robots"],
      ["browser.startup.page", 1],
    ],
  });

  // Wait for Activity Stream to add its panels
  await BrowserTestUtils.waitForCondition(() =>
    SpecialPowers.spawn(
      gBrowser.selectedTab.linkedBrowser,
      [],
      () => !!content.document.getElementById("homeContentsGroup")
    )
  );

  await evaluateSearchResults("Set Home Page", "homepageGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/**
 * Test for searching for the "Languages" subdialog.
 */
add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  await evaluateSearchResults("Choose languages", "languagesGroup");
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
