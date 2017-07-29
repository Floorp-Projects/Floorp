/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(3);

add_task(async function test_remove_all_tour_notifications_through_close_button() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tourIds = TOUR_IDs;
  let tab = null;
  let targetTourId = null;
  await closeTourNotificationsOneByOne();

  let expectedPrefUpdate = promisePrefUpdated("browser.onboarding.notification.finished", true);
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await expectedPrefUpdate;
  let tourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  ok(!tourId, "Should not prompt tour notifications any more after closing all notifcations.");
  await BrowserTestUtils.removeTab(tab);

  async function closeTourNotificationsOneByOne() {
    for (let i = 0; i < tourIds.length; ++i) {
      if (tab) {
        await reloadTab(tab);
      } else {
        tab = await openTab(ABOUT_NEWTAB_URL);
      }
      await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
      await promiseTourNotificationOpened(tab.linkedBrowser);
      targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
      is(targetTourId, tourIds[i], `Should show tour notifications of ${targetTourId}`);
      await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-notification-close-btn", {}, tab.linkedBrowser);
      await promiseTourNotificationClosed(tab.linkedBrowser);
    }
  }
});

add_task(async function test_remove_all_tour_notifications_through_action_button() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tourIds = TOUR_IDs;
  let tab = null;
  let targetTourId = null;
  await clickTourNotificationActionButtonsOneByOne();

  let expectedPrefUpdate = promisePrefUpdated("browser.onboarding.notification.finished", true);
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await expectedPrefUpdate;
  let tourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  ok(!tourId, "Should not prompt tour notifcations any more after taking actions on all notifcations.");
  await BrowserTestUtils.removeTab(tab);

  async function clickTourNotificationActionButtonsOneByOne() {
    for (let i = 0; i < tourIds.length; ++i) {
      if (tab) {
        await reloadTab(tab);
      } else {
        tab = await openTab(ABOUT_NEWTAB_URL);
      }
      await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
      await promiseTourNotificationOpened(tab.linkedBrowser);
      targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
      is(targetTourId, tourIds[i], `Should show tour notifications of ${targetTourId}`);
      await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-notification-action-btn", {}, tab.linkedBrowser);
      await promiseTourNotificationClosed(tab.linkedBrowser);
    }
  }
});
