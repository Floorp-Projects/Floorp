/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const OVERLAY_ICON_ID = "#onboarding-overlay-button";
const PRIVATE_BROWSING_TOUR_ID = "#onboarding-tour-private-browsing";
const ADDONS_TOUR_ID = "#onboarding-tour-addons";
const CUSTOMIZE_TOUR_ID = "#onboarding-tour-customize";
const CLASS_ACTIVE = "onboarding-active";

add_task(async function test_default_tour_open_the_right_page() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "new"],
    ["browser.onboarding.tourset-version", 1],
    ["browser.onboarding.seen-tourset-version", 1],
    ["browser.onboarding.newtour", "private,addons,customize"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(OVERLAY_ICON_ID, {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  info("Make sure the default tour is active and open the right page");
  let { activeNavItemId, activePageId } = await getCurrentActiveTour(tab.linkedBrowser);
  is(`#${activeNavItemId}`, PRIVATE_BROWSING_TOUR_ID, "default tour is active");
  is(activePageId, "onboarding-tour-private-browsing-page", "default tour page is shown");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_first_tour_is_active_by_default() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "new"],
    ["browser.onboarding.tourset-version", 1],
    ["browser.onboarding.seen-tourset-version", 1],
    ["browser.onboarding.newtour", "private,addons,customize"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(OVERLAY_ICON_ID, {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  info("Make sure the default tour is selected");
  let doc = content && content.document;
  let dom = doc.querySelector(PRIVATE_BROWSING_TOUR_ID);
  ok(dom.classList.contains(CLASS_ACTIVE), "default tour is selected");
  let dom2 = doc.querySelector(ADDONS_TOUR_ID);
  ok(!dom2.classList.contains(CLASS_ACTIVE), "none default tour should not be selected");
  let dom3 = doc.querySelector(CUSTOMIZE_TOUR_ID);
  ok(!dom3.classList.contains(CLASS_ACTIVE), "none default tour should not be selected");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_select_first_uncomplete_tour() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "new"],
    ["browser.onboarding.tourset-version", 1],
    ["browser.onboarding.seen-tourset-version", 1],
    ["browser.onboarding.newtour", "private,addons,customize"],
  ]});
  setTourCompletedState("onboarding-tour-private-browsing", true);

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(OVERLAY_ICON_ID, {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  info("Make sure the first uncomplete tour is selected");
  let doc = content && content.document;
  let dom = doc.querySelector(PRIVATE_BROWSING_TOUR_ID);
  ok(!dom.classList.contains(CLASS_ACTIVE), "the first tour is set completed and should not be selected");
  let dom2 = doc.querySelector(ADDONS_TOUR_ID);
  ok(dom2.classList.contains(CLASS_ACTIVE), "the first uncomplete tour is selected");
  let dom3 = doc.querySelector(CUSTOMIZE_TOUR_ID);
  ok(!dom3.classList.contains(CLASS_ACTIVE), "other tour should not be selected");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_select_first_tour_when_all_tours_are_complete() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "new"],
    ["browser.onboarding.tourset-version", 1],
    ["browser.onboarding.seen-tourset-version", 1],
    ["browser.onboarding.newtour", "private,addons,customize"],
  ]});
  setTourCompletedState("onboarding-tour-private-browsing", true);
  setTourCompletedState("onboarding-tour-addons", true);
  setTourCompletedState("onboarding-tour-customize", true);

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter(OVERLAY_ICON_ID, {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  info("Make sure the first tour is selected when all tours are completed");
  let doc = content && content.document;
  let dom = doc.querySelector(PRIVATE_BROWSING_TOUR_ID);
  ok(dom.classList.contains(CLASS_ACTIVE), "should be selected when all tours are completed");
  let dom2 = doc.querySelector(ADDONS_TOUR_ID);
  ok(!dom2.classList.contains(CLASS_ACTIVE), "other tour should not be selected");
  let dom3 = doc.querySelector(CUSTOMIZE_TOUR_ID);
  ok(!dom3.classList.contains(CLASS_ACTIVE), "other tour should not be selected");

  await BrowserTestUtils.removeTab(tab);
});
