/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(3);

function promisePopupChange(popup, expectedState) {
  return new Promise(resolve => {
    let event = expectedState == "open" ? "popupshown" : "popuphidden";
    popup.addEventListener(event, resolve, { once: true });
  });
}

async function promiseOpenOnboardingOverlay(tab) {
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button", {}, tab.linkedBrowser);
  return promiseOnboardingOverlayOpened(tab.linkedBrowser);
}

async function triggerUITourHighlight(tourName, tab) {
  await promiseOpenOnboardingOverlay(tab);
  BrowserTestUtils.synthesizeMouseAtCenter(`#onboarding-tour-${tourName}`, {}, tab.linkedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter(`#onboarding-tour-${tourName}-button`, {}, tab.linkedBrowser);
}

add_task(async function test_clean_up_uitour_after_closing_overlay() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "library"],
  ]});

  // Trigger UITour showHighlight
  let highlight = document.getElementById("UITourHighlightContainer");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  let tab = await openTab(ABOUT_NEWTAB_URL);
  await triggerUITourHighlight("library", tab);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "library", "UITour should highlight library");

  // Close the overlay by clicking the overlay
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  BrowserTestUtils.synthesizeMouseAtPoint(2, 2, {}, tab.linkedBrowser);
  await promiseOnboardingOverlayClosed(tab.linkedBrowser);
  await highlightClosePromise;
  is(highlight.state, "closed", "Should close UITour highlight after closing the overlay by clicking the overlay");

  // Trigger UITour showHighlight
  highlightOpenPromise = promisePopupChange(highlight, "open");
  await triggerUITourHighlight("library", tab);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "library", "UITour should highlight library");

  // Close the overlay by clicking the overlay close button
  highlightClosePromise = promisePopupChange(highlight, "closed");
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-close-btn", {}, tab.linkedBrowser);
  await promiseOnboardingOverlayClosed(tab.linkedBrowser);
  await highlightClosePromise;
  is(highlight.state, "closed", "Should close UITour highlight after closing the overlay by clicking the overlay close button");

  // Trigger UITour showHighlight again
  highlightOpenPromise = promisePopupChange(highlight, "open");
  await triggerUITourHighlight("library", tab);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "library", "UITour should highlight library");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_clean_up_uitour_after_navigating_to_other_tour_by_keyboard() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "singlesearch,customize"],
  ]});

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOpenOnboardingOverlay(tab);

  // Navigate to the Customize tour to trigger UITour showHighlight
  let highlight = document.getElementById("UITourHighlightContainer");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  tab.linkedBrowser.focus(); // Make sure the key event will be fired on the focused page
  await BrowserTestUtils.synthesizeKey("VK_TAB", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("VK_TAB", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("VK_TAB", {}, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "customize", "UITour should highlight customize");

  // Navigate to the Single-Search tour
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  tab.linkedBrowser.focus(); // Make sure the key event will be fired on the focused page
  await BrowserTestUtils.synthesizeKey("VK_TAB", { shiftKey: true }, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("VK_TAB", { shiftKey: true }, tab.linkedBrowser);
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await highlightClosePromise;
  is(highlight.state, "closed", "Should close UITour highlight after navigating to another tour by keyboard");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_clean_up_uitour_after_navigating_to_other_tour_by_mouse() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "singlesearch,customize"],
  ]});

  // Navigate to the Customize tour to trigger UITour showHighlight
  let highlight = document.getElementById("UITourHighlightContainer");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  let tab = await openTab(ABOUT_NEWTAB_URL);
  await triggerUITourHighlight("customize", tab);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "customize", "UITour should highlight customize");

  // Navigate to the Single-Search tour
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-tour-singlesearch", {}, tab.linkedBrowser);
  await highlightClosePromise;
  is(highlight.state, "closed", "Should close UITour highlight after navigating to another tour by mouse");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_clean_up_uitour_on_page_unload() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "singlesearch,customize"],
  ]});

  // Trigger UITour showHighlight
  let highlight = document.getElementById("UITourHighlightContainer");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  let tab = await openTab(ABOUT_NEWTAB_URL);
  await triggerUITourHighlight("customize", tab);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "customize", "UITour should highlight customize");

  // Load another page to unload the current page
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com");
  await highlightClosePromise;
  is(highlight.state, "closed", "Should close UITour highlight after page unloaded");
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_clean_up_uitour_on_window_resize() {
  resetOnboardingDefaultState();
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.onboarding.newtour", "singlesearch,customize"],
  ]});

  // Trigger UITour showHighlight
  let highlight = document.getElementById("UITourHighlightContainer");
  let highlightOpenPromise = promisePopupChange(highlight, "open");
  let tab = await openTab(ABOUT_NEWTAB_URL);
  await triggerUITourHighlight("customize", tab);
  await highlightOpenPromise;
  is(highlight.state, "open", "Should show UITour highlight");
  is(highlight.getAttribute("targetName"), "customize", "UITour should highlight customize");

  // Resize window to destroy the onboarding tour
  const originalWidth = window.innerWidth;
  let highlightClosePromise = promisePopupChange(highlight, "closed");
  window.innerWidth = 300;
  await highlightClosePromise;
  is(highlight.state, "closed", "Should close UITour highlight after window resized");
  window.innerWidth = originalWidth;
  await BrowserTestUtils.removeTab(tab);
});
