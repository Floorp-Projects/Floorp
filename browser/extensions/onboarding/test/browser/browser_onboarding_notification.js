/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_show_tour_notifications_in_order() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.enabled", true]]});

  let tourIds = TOUR_IDs;
  let tab = null;
  let targetTourId = null;
  let reloadPromise = null;
  let expectedPrefUpdate = null;
  for (let i = 0; i < tourIds.length; ++i) {
    expectedPrefUpdate = promisePrefUpdated("browser.onboarding.notification.lastPrompted", tourIds[i]);
    if (tab) {
      reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
      tab.linkedBrowser.reload();
      await reloadPromise;
    } else {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
      await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
    }
    await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
    await promiseTourNotificationOpened(tab.linkedBrowser);
    targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
    is(targetTourId, tourIds[i], "Should show tour notifications in order");
    await expectedPrefUpdate;
  }

  expectedPrefUpdate = promisePrefUpdated("browser.onboarding.notification.lastPrompted", tourIds[0]);
  reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await reloadPromise;
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, tourIds[0], "Should loop back to the 1st tour notification after showing all notifications");
  await expectedPrefUpdate;
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_open_target_tour_from_notification() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.enabled", true]]});

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-notification-action-btn", {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);
  let { activeNavItemId, activePageId } = await getCurrentActiveTour(tab.linkedBrowser);

  is(targetTourId, activeNavItemId, "Should navigate to the target tour item.");
  is(`${targetTourId}-page`, activePageId, "Should display the target tour page.");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_not_show_notification_for_completed_tour() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.enabled", true]]});

  let tourIds = TOUR_IDs;
  // Make only the last tour uncompleted
  let lastTourId = tourIds[tourIds.length - 1];
  for (let id of tourIds) {
    if (id != lastTourId) {
      setTourCompletedState(id, true);
    }
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, lastTourId, "Should not show notification for completed tour");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_skip_notification_for_completed_tour() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [["browser.onboarding.enabled", true]]});

  let tourIds = TOUR_IDs;
  // Make only 2nd tour completed
  await setTourCompletedState(tourIds[1], true);

  // Test show notification for the 1st tour
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, tourIds[0], "Should show notification for incompleted tour");

  // Test skip the 2nd tour and show notification for the 3rd tour
  let reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await reloadPromise;
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, tourIds[2], "Should skip notification for the completed 2nd tour");
  await BrowserTestUtils.removeTab(tab);
});
