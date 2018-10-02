/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

async function testTourIDs(browser, tourIDs) {
  await ContentTask.spawn(browser, tourIDs, async (tourIDsContent) => {
    let doc = content.document;
    let doms = doc.querySelectorAll(".onboarding-tour-item");
    Assert.equal(doms.length, tourIDsContent.length, "has exact tour numbers");
    doms.forEach((dom, idx) => {
      Assert.equal(tourIDsContent[idx], dom.id, "contain defined onboarding id");
    });
  });
}

add_task(async function test_onboarding_default_new_tourset() {
  resetOnboardingDefaultState();

  let tab = await openTab(ABOUT_NEWTAB_URL);
  let browser = tab.linkedBrowser;
  await promiseOnboardingOverlayLoaded(browser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, browser);
  await promiseOnboardingOverlayOpened(browser);

  await testTourIDs(browser, TOUR_IDs);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_onboarding_custom_new_tourset() {
  const CUSTOM_NEW_TOURs = [
    "onboarding-tour-private-browsing",
    "onboarding-tour-addons",
    "onboarding-tour-customize",
  ];

  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "new"],
    ["browser.onboarding.tourset-version", 1],
    ["browser.onboarding.seen-tourset-version", 1],
    ["browser.onboarding.newtour", "private,addons,customize"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  let browser = tab.linkedBrowser;
  await promiseOnboardingOverlayLoaded(browser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, browser);
  await promiseOnboardingOverlayOpened(browser);

  await testTourIDs(browser, CUSTOM_NEW_TOURs);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_onboarding_custom_update_tourset() {
  const CUSTOM_UPDATE_TOURs = [
    "onboarding-tour-customize",
    "onboarding-tour-private-browsing",
    "onboarding-tour-addons",
  ];
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "update"],
    ["browser.onboarding.tourset-version", 1],
    ["browser.onboarding.seen-tourset-version", 1],
    ["browser.onboarding.updatetour", "customize,private,addons"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  let browser = tab.linkedBrowser;
  await promiseOnboardingOverlayLoaded(browser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, browser);
  await promiseOnboardingOverlayOpened(browser);

  await testTourIDs(browser, CUSTOM_UPDATE_TOURs);

  BrowserTestUtils.removeTab(tab);
});
