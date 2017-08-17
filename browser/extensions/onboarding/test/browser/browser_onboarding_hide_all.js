/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 "use strict";

function assertOnboardingDestroyed(browser) {
  return ContentTask.spawn(browser, {}, function() {
    let expectedRemovals = [
      "#onboarding-overlay",
      "#onboarding-overlay-button"
    ];
    for (let selector of expectedRemovals) {
      let removal = content.document.querySelector(selector);
      ok(!removal, `Should remove ${selector} onboarding element`);
    }
  });
}

add_task(async function test_hide_onboarding_tours() {
  resetOnboardingDefaultState();

  let tourIds = TOUR_IDs;
  let expectedPrefUpdates = [
    promisePrefUpdated("browser.onboarding.hidden", true),
    promisePrefUpdated("browser.onboarding.notification.finished", true)
  ];
  tourIds.forEach((id, idx) => expectedPrefUpdates.push(promisePrefUpdated(`browser.onboarding.tour.${id}.completed`, true)));

  let tabs = [];
  for (let url of URLs) {
    let tab = await openTab(url);
    await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
    await promiseOnboardingOverlayOpened(tab.linkedBrowser);
    tabs.push(tab);
  }

  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-hidden-checkbox", {}, gBrowser.selectedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-close-btn", {}, gBrowser.selectedBrowser);
  await Promise.all(expectedPrefUpdates);

  for (let i = tabs.length - 1; i >= 0; --i) {
    let tab = tabs[i];
    await assertOnboardingDestroyed(tab.linkedBrowser);
    await BrowserTestUtils.removeTab(tab);
  }
});
