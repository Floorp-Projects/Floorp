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

function promisePopupChange(popup, expectedState) {
  return new Promise(resolve => {
    let event = expectedState == "open" ? "popupshown" : "popuphidden";
    popup.addEventListener(event, resolve, { once: true });
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
    await BrowserTestUtils.removeTab(tab);
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
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_clean_up_uitour_on_page_unload() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "singlesearch,customize"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  // Trigger UITour showHighlight and showMenu
  let highlight = document.getElementById("UITourHighlightContainer");
  let urlbarOpenPromise = promisePopupChange(gURLBar.popup, "open");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-singlesearch", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-singlesearch-button", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-customize", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-customize-button", {}, tab.linkedBrowser);
  await urlbarOpenPromise;
  await highlightOpenPromise;
  is(gURLBar.popup.state, "open", "Should show urlbar popup");
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "customize", "UITour should highlight customize");

  // Load another page to unload the current page
  let urlbarClosePromise = promisePopupChange(gURLBar.popup, "closed");
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com");
  await urlbarClosePromise;
  await highlightClosePromise;
  is(gURLBar.popup.state, "closed", "Should close urlbar popup after page unloaded");
  is(highlight.state, "closed", "Should close UITour highlight after page unloaded");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_clean_up_uitour_on_window_resize() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "singlesearch,customize"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  // Trigger UITour showHighlight and showMenu
  let highlight = document.getElementById("UITourHighlightContainer");
  let urlbarOpenPromise = promisePopupChange(gURLBar.popup, "open");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-singlesearch", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-singlesearch-button", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-customize", {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-customize-button", {}, tab.linkedBrowser);
  await urlbarOpenPromise;
  await highlightOpenPromise;
  is(gURLBar.popup.state, "open", "Should show urlbar popup");
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "customize", "UITour should highlight customize");

  // Resize window to destroy the onboarding tour
  const originalWidth = window.innerWidth;
  let urlbarClosePromise = promisePopupChange(gURLBar.popup, "closed");
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  window.innerWidth = 300;
  await urlbarClosePromise;
  await highlightClosePromise;
  is(gURLBar.popup.state, "closed", "Should close urlbar popup after window resized");
  is(highlight.state, "closed", "Should close UITour highlight after window resized");

  window.innerWidth = originalWidth;
  await BrowserTestUtils.removeTab(tab);
});
