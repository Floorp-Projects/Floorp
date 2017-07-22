/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(3);

add_task(async function test_not_show_notification_for_completed_tour() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tourIds = TOUR_IDs;
  // Make only the last tour uncompleted
  let lastTourId = tourIds[tourIds.length - 1];
  for (let id of tourIds) {
    if (id != lastTourId) {
      setTourCompletedState(id, true);
    }
  }

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, lastTourId, "Should not show notification for completed tour");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_skip_notification_for_completed_tour() {
  resetOnboardingDefaultState();
  Preferences.set("browser.onboarding.notification.max-prompt-count-per-tour", 1);
  skipMuteNotificationOnFirstSession();

  let tourIds = TOUR_IDs;
  // Make only 2nd tour completed
  await setTourCompletedState(tourIds[1], true);

  // Test show notification for the 1st tour
  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, tourIds[0], "Should show notification for incompleted tour");

  // Test skip the 2nd tour and show notification for the 3rd tour
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is(targetTourId, tourIds[2], "Should skip notification for the completed 2nd tour");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_mute_notification_on_1st_session() {
  resetOnboardingDefaultState();

  // Test no notifications during the mute duration on the 1st session
  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  // The tour notification would be prompted on idle, so we wait idle twice here before proceeding
  await waitUntilWindowIdle(tab.linkedBrowser);
  await waitUntilWindowIdle(tab.linkedBrowser);
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await waitUntilWindowIdle(tab.linkedBrowser);
  await waitUntilWindowIdle(tab.linkedBrowser);
  let promptCount = Preferences.get("browser.onboarding.notification.prompt-count", 0);
  is(0, promptCount, "Should not prompt tour notification during the mute duration on the 1st session");

  // Test notification prompted after the mute duration on the 1st session
  let muteTime = Preferences.get("browser.onboarding.notification.mute-duration-on-first-session-ms");
  let lastTime = Math.floor((Date.now() - muteTime - 1) / 1000);
  Preferences.set("browser.onboarding.notification.last-time-of-changing-tour-sec", lastTime);
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  promptCount = Preferences.get("browser.onboarding.notification.prompt-count", 0);
  is(1, promptCount, "Should prompt tour notification after the mute duration on the 1st session");
  await BrowserTestUtils.removeTab(tab);
});
