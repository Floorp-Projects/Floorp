add_task(async function test_show_click_auto_complete_tour_in_notification() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();
  // the second tour is an click-auto-complete tour
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "customize,library"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  // Trigger CTA button to mark the tour as complete
  let expectedPrefUpdates = [
    promisePrefUpdated(`browser.onboarding.tour.onboarding-tour-customize.completed`, true),
  ];
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-customize", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-customize-button", {}, tab.linkedBrowser);
  await Promise.all(expectedPrefUpdates);

  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-close-btn", {}, gBrowser.selectedBrowser);
  let { activeNavItemId } = await getCurrentActiveTour(tab.linkedBrowser);
  is("onboarding-tour-customize", activeNavItemId, "the active tour should be the previous shown tour");

  await reloadTab(tab);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);
  let targetTourId = await getCurrentNotificationTargetTourId(tab.linkedBrowser);
  is("onboarding-tour-library", targetTourId, "correctly show the click-autocomplete-tour in notification");

  await BrowserTestUtils.removeTab(tab);
});
