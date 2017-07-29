/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(3);

add_task(async function test_show_tour_notifications_in_order() {
  resetOnboardingDefaultState();
  Preferences.set("browser.onboarding.notification.max-prompt-count-per-tour", 1);
  skipMuteNotificationOnFirstSession();

  let tourIds = TOUR_IDs;
  let tab = null;
  let targetTourId = null;
  let expectedPrefUpdate = null;
  await loopTourNotificationQueueOnceInOrder();
  await loopTourNotificationQueueOnceInOrder();

  expectedPrefUpdate = promisePrefUpdated("browser.onboarding.notification.finished", true);
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await expectedPrefUpdate;
  let tourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  ok(!tourId, "Should not prompt each tour for more than 2 chances.");
  await BrowserTestUtils.removeTab(tab);

  async function loopTourNotificationQueueOnceInOrder() {
    for (let i = 0; i < tourIds.length; ++i) {
      if (tab) {
        await reloadTab(tab);
      } else {
        tab = await openTab(ABOUT_NEWTAB_URL);
      }
      await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
      await promiseTourNotificationOpened(tab.linkedBrowser);
      targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
      is(targetTourId, tourIds[i], "Should show tour notifications in order");
    }
  }
});

add_task(async function test_open_target_tour_from_notification() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tab = await openTab(ABOUT_NEWTAB_URL);
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
