/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_finish_tour_notifcations_after_total_max_life_time() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);

  let totalMaxTime = Preferences.get("browser.onboarding.notification.max-life-time-all-tours-ms");
  Preferences.set("browser.onboarding.notification.last-time-of-changing-tour-sec", Math.floor((Date.now() - totalMaxTime) / 1000));
  let expectedPrefUpdate = promisePrefUpdated("browser.onboarding.notification.finished", true);
  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await expectedPrefUpdate;
  await BrowserTestUtils.removeTab(tab);
});
