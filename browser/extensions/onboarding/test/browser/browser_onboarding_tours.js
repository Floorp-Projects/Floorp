/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 "use strict";

requestLongerTimeout(2);

function assertTourCompleted(tourId, expectComplete, browser) {
  return ContentTask.spawn(browser, { tourId, expectComplete }, function(args) {
    let item = content.document.querySelector(`#${args.tourId}.onboarding-tour-item`);
    let completedTextId = `onboarding-complete-${args.tourId}-text`;
    let completedText = item.querySelector(`#${completedTextId}`);
    if (args.expectComplete) {
      ok(item.classList.contains("onboarding-complete"), `Should set the complete #${args.tourId} tour with the complete style`);
      ok(completedText, "Text label should be present for a completed item");
      is(completedText.id, completedTextId, "Text label node should have a unique id");
      ok(completedText.getAttribute("aria-label"), "Text label node should have an aria-label attribute set");
      is(item.getAttribute("aria-describedby"), completedTextId,
        "Completed item should have aria-describedby attribute set to text label node's id");
    } else {
      ok(!item.classList.contains("onboarding-complete"), `Should not set the incomplete #${args.tourId} tour with the complete style`);
      ok(!completedText, "Text label should not be present for an incomplete item");
      ok(!item.hasAttribute("aria-describedby"), "Incomplete item should not have aria-describedby attribute set");
    }
  });
}

add_task(async function test_set_right_tour_completed_style_on_overlay() {
  resetOnboardingDefaultState();

  let tourIds = TOUR_IDs;
  // Mark the tours of even number as completed
  for (let i = 0; i < tourIds.length; ++i) {
    setTourCompletedState(tourIds[i], i % 2 == 0);
  }

  let tabs = [];
  for (let url of URLs) {
    let tab = await openTab(url);
    await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
    await promiseOnboardingOverlayOpened(tab.linkedBrowser);
    tabs.push(tab);
  }

  for (let i = tabs.length - 1; i >= 0; --i) {
    let tab = tabs[i];
    await assertOverlaySemantics(tab.linkedBrowser);
    for (let j = 0; j < tourIds.length; ++j) {
      await assertTourCompleted(tourIds[j], j % 2 == 0, tab.linkedBrowser);
    }
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_click_action_button_to_set_tour_completed() {
  resetOnboardingDefaultState();
  const CUSTOM_TOUR_IDs = [
    "onboarding-tour-private-browsing",
    "onboarding-tour-addons",
    "onboarding-tour-customize",
  ];
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "private,addons,customize"],
  ]});

  let tourIds = CUSTOM_TOUR_IDs;
  let tabs = [];
  for (let url of URLs) {
    let tab = await openTab(url);
    await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
    await promiseOnboardingOverlayOpened(tab.linkedBrowser);
    tabs.push(tab);
  }

  let completedTourId = tourIds[0];
  let expectedPrefUpdate = promisePrefUpdated(`browser.onboarding.tour.${completedTourId}.completed`, true);
  await BrowserTestUtils.synthesizeMouseAtCenter(`#${completedTourId}-page .onboarding-tour-action-button`, {}, gBrowser.selectedBrowser);
  await expectedPrefUpdate;

  for (let i = tabs.length - 1; i >= 0; --i) {
    let tab = tabs[i];
    await assertOverlaySemantics(tab.linkedBrowser);
    for (let id of tourIds) {
      await assertTourCompleted(id, id == completedTourId, tab.linkedBrowser);
    }
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_set_watermark_after_all_tour_completed() {
  resetOnboardingDefaultState();

  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.tour-type", "new"],
  ]});

  let tabs = [];
  for (let url of URLs) {
    let tab = await openTab(url);
    await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
    await promiseOnboardingOverlayOpened(tab.linkedBrowser);
    tabs.push(tab);
  }
  let expectedPrefUpdate = promisePrefUpdated("browser.onboarding.state", ICON_STATE_WATERMARK);
  TOUR_IDs.forEach(id => Preferences.set(`browser.onboarding.tour.${id}.completed`, true));
  await expectedPrefUpdate;

  for (let tab of tabs) {
    await assertWatermarkIconDisplayed(tab.linkedBrowser);
    BrowserTestUtils.removeTab(tab);
  }
});
