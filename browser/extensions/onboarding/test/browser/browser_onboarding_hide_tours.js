/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function assertOnboardingDestroyed(browser) {
  return ContentTask.spawn(browser, {}, function() {
    let expectedRemovals = [
      "#onboarding-overlay",
      "#onboarding-overlay-icon"
    ];
    for (let selector of expectedRemovals) {
      let removal = content.document.querySelector(selector);
      ok(!removal, `Should remove ${selector} onboarding element`);
    }
  });
}

add_task(async function test_hide_onboarding_tours() {
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.enabled", true]]});
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.hidden", false]]});
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.notification.finished", false]]});

  let newtab = await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(newtab.linkedBrowser);
  let hometab = await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_HOME_URL);
  await promiseOnboardingOverlayLoaded(hometab.linkedBrowser);

  let expectedPrefUpdates = [
    promisePrefUpdated("browser.onboarding.hidden", true),
    promisePrefUpdated("browser.onboarding.notification.finished", true)
  ];
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-icon", {}, hometab.linkedBrowser);
  await promiseOnboardingOverlayOpened(hometab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-hidden-checkbox", {}, hometab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-close-btn", {}, hometab.linkedBrowser);
  await Promise.all(expectedPrefUpdates);

  // Test the hiding operation works arcoss pages
  await assertOnboardingDestroyed(hometab.linkedBrowser);
  await BrowserTestUtils.removeTab(hometab);
  await assertOnboardingDestroyed(newtab.linkedBrowser);
  await BrowserTestUtils.removeTab(newtab);
});
