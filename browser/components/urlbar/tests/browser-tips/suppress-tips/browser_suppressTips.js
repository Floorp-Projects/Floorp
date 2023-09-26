/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the browser tips are suppressed correctly.

"use strict";

/* import-globals-from ../head.js */

ChromeUtils.defineESModuleGetters(this, {
  LaterRun: "resource:///modules/LaterRun.sys.mjs",
  UrlbarProviderSearchTips:
    "resource:///modules/UrlbarProviderSearchTips.sys.mjs",
});

const LAST_UPDATE_THRESHOLD_HOURS = 24;

add_setup(async function () {
  await PlacesUtils.history.clear();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`,
        0,
      ],
    ],
  });

  registerCleanupFunction(() => {
    resetSearchTipsProvider();
    Services.prefs.clearUserPref(
      "browser.laterrun.bookkeeping.profileCreationTime"
    );
    Services.prefs.clearUserPref(
      "browser.laterrun.bookkeeping.updateAppliedTime"
    );
  });
});

add_task(async function updateApplied() {
  // Check the update time.
  Assert.notEqual(
    Services.prefs.getIntPref(
      "browser.laterrun.bookkeeping.updateAppliedTime",
      0
    ),
    0,
    "updateAppliedTime pref should be updated when booting this test"
  );
  Assert.equal(
    LaterRun.hoursSinceUpdate,
    0,
    "LaterRun.hoursSinceUpdate is 0 since one hour should not have passed from starting this test"
  );

  // To not suppress the tip by profile creation.
  Services.prefs.setIntPref(
    "browser.laterrun.bookkeeping.profileCreationTime",
    secondsBasedOnNow(LAST_UPDATE_THRESHOLD_HOURS + 0.5)
  );

  // The test harness will use the current tab and remove the tab's history.
  // Since the page that is tested is opened prior to the test harness taking
  // over the current tab the active-update.xml specifies two pages to open by
  // having 'https://example.com/|https://example.com/' for the value of openURL
  // and then uses the first tab for the test.
  gBrowser.selectedTab = gBrowser.tabs[0];
  // The test harness also changes the page to about:blank so go back to the
  // page that was originally opened.
  gBrowser.goBack();
  // Wait for the page to go back to the original page.
  await TestUtils.waitForCondition(
    () => gBrowser.selectedBrowser?.currentURI?.spec == "https://example.com/",
    "Waiting for the expected page to reopen"
  );
  gBrowser.removeTab(gBrowser.selectedTab);

  // Check whether the tip is suppressed by update.
  await checkTab(window, "about:newtab");

  // Clean up.
  const alternatePath = Services.prefs.getCharPref(
    "app.update.altUpdateDirPath"
  );
  const testRoot = Services.prefs.getCharPref("mochitest.testRoot");
  let relativePath = alternatePath.substring("<test-root>".length);
  if (AppConstants.platform == "win") {
    relativePath = relativePath.replace(/\//g, "\\");
  }
  const updateDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  updateDir.initWithPath(testRoot + relativePath);
  const updatesFile = updateDir.clone();
  updatesFile.append("updates.xml");
  await TestUtils.waitForCondition(
    () => updatesFile.exists(),
    "Waiting until the updates.xml file exists"
  );
  updatesFile.remove(false);
});

add_task(async function profileAge() {
  // To not suppress the tip by profile creation and update.
  Services.prefs.setIntPref(
    "browser.laterrun.bookkeeping.profileCreationTime",
    secondsBasedOnNow(LAST_UPDATE_THRESHOLD_HOURS + 0.5)
  );
  Services.prefs.setIntPref(
    "browser.laterrun.bookkeeping.updateAppliedTime",
    secondsBasedOnNow(LAST_UPDATE_THRESHOLD_HOURS + 0.5)
  );
  await checkTab(
    window,
    "about:newtab",
    UrlbarProviderSearchTips.TIP_TYPE.ONBOARD
  );

  // To suppress the tip by profile creation.
  Services.prefs.setIntPref(
    "browser.laterrun.bookkeeping.profileCreationTime",
    secondsBasedOnNow()
  );
  await checkTab(window, "about:newtab");
});

function secondsBasedOnNow(howManyHoursAgo = 0) {
  return Math.floor(Date.now() / 1000 - howManyHoursAgo * 60 * 60);
}
