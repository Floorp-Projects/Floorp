/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_move_on_to_next_notification_when_reaching_max_prompt_count() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();
  let maxCount = Preferences.get("browser.onboarding.notification.max-prompt-count-per-tour");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let previousTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);

  let currentTourId = null;
  let reloadPromise = null;
  for (let i = maxCount - 1; i > 0; --i) {
    reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tab.linkedBrowser.reload();
    await reloadPromise;
    await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
    await promiseTourNotificationOpened(tab.linkedBrowser);
    currentTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
    is(previousTourId, currentTourId, "Should not move on to next tour notification until reaching the max prompt count per tour");
  }

  reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await reloadPromise;
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  currentTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  isnot(previousTourId, currentTourId, "Should move on to next tour notification when reaching the max prompt count per tour");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_move_on_to_next_notification_when_reaching_max_life_time() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let previousTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);

  let maxTime = Preferences.get("browser.onboarding.notification.max-life-time-per-tour-ms");
  let lastTime = Math.floor((Date.now() - maxTime - 1) / 1000);
  Preferences.set("browser.onboarding.notification.last-time-of-changing-tour-sec", lastTime);
  let reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await reloadPromise;
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let currentTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  isnot(previousTourId, currentTourId, "Should move on to next tour notification when reaching the max life time per tour");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_move_on_to_next_notification_after_interacting_with_notification() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let previousTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-notification-close-btn", {}, tab.linkedBrowser);

  let reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await reloadPromise;
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let currentTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  isnot(previousTourId, currentTourId, "Should move on to next tour notification after clicking #onboarding-notification-close-btn");
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-notification-action-btn", {}, tab.linkedBrowser);
  previousTourId = currentTourId;

  reloadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await reloadPromise;
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  currentTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  isnot(previousTourId, currentTourId, "Should move on to next tour notification after clicking #onboarding-notification-action-btn");

  await BrowserTestUtils.removeTab(tab);
});
