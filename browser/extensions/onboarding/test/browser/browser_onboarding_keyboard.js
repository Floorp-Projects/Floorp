/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

function assertOverlayState(browser, args) {
  return ContentTask.spawn(browser, args, ({ tourId, focusedId, visible }) => {
    let { document: doc, window} = content;
    if (tourId) {
      let items = [...doc.querySelectorAll(".onboarding-tour-item")];
      items.forEach(item => is(item.getAttribute("aria-selected"),
        item.id === tourId ? "true" : "false",
        "Active item should have aria-selected set to true and inactive to false"));
    }
    if (focusedId) {
      let focused = doc.getElementById(focusedId);
      is(focused, doc.activeElement, `Focus should be set on ${focusedId}`);
    }
    if (visible !== undefined) {
      let overlay = doc.getElementById("onboarding-overlay");
      is(window.getComputedStyle(overlay).getPropertyValue("display"),
        visible ? "block" : "none",
        `Onboarding overlay should be ${visible ? "visible" : "invisible"}`);
    }
  });
}

const TOUR_LIST_TEST_DATA = [
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[1], focusedId: TOUR_IDs[1] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[2], focusedId: TOUR_IDs[2] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[3] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[4], focusedId: TOUR_IDs[4] }},
  { key: "VK_UP", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[3] }},
  { key: "VK_UP", expected: { tourId: TOUR_IDs[2], focusedId: TOUR_IDs[2] }},
  { key: "VK_TAB", expected: { tourId: TOUR_IDs[2], focusedId: TOUR_IDs[3] }},
  { key: "VK_TAB", expected: { tourId: TOUR_IDs[2], focusedId: TOUR_IDs[4] }},
  { key: "VK_RETURN", expected: { tourId: TOUR_IDs[4], focusedId: TOUR_IDs[4] }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { tourId: TOUR_IDs[4], focusedId: TOUR_IDs[3] }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { tourId: TOUR_IDs[4], focusedId: TOUR_IDs[2] }},
  // VK_SPACE does not work well with EventUtils#synthesizeKey use " " instead
  { key: " ", expected: { tourId: TOUR_IDs[2], focusedId: TOUR_IDs[2] }},
];

const BUTTONS_TEST_DATA = [
  { key: " ", expected: { focusedId: TOUR_IDs[0], visible: true }},
  { key: "VK_ESCAPE", expected: { focusedId: "onboarding-overlay-button", visible: false }},
  { key: "VK_RETURN", expected: { focusedId: TOUR_IDs[1], visible: true }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { focusedId: TOUR_IDs[0], visible: true }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { focusedId: "onboarding-overlay-close-btn", visible: true }},
  { key: " ", expected: { focusedId: "onboarding-overlay-button", visible: false }},
  { key: "VK_RETURN", expected: { focusedId: TOUR_IDs[1], visible: true }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { focusedId: TOUR_IDs[0], visible: true }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { focusedId: "onboarding-overlay-close-btn", visible: true }},
  { key: "VK_TAB", expected: { focusedId: TOUR_IDs[0], visible: true }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { focusedId: "onboarding-overlay-close-btn", visible: true }},
  { key: "VK_RETURN", expected: { focusedId: "onboarding-overlay-button", visible: false }},
];

add_task(async function test_tour_list_keyboard_navigation() {
  resetOnboardingDefaultState();

  info("Display onboarding overlay on the home page");
  let tab = await openTab(ABOUT_HOME_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button",
                                                 {}, tab.linkedBrowser);
  await promiseOnboardingOverlayOpened(tab.linkedBrowser);

  info("Checking overall overlay tablist semantics");
  await assertOverlaySemantics(tab.linkedBrowser);

  info("Set initial focus on the currently active tab");
  await ContentTask.spawn(tab.linkedBrowser, {}, () =>
    content.document.querySelector(".onboarding-active").focus());
  await assertOverlayState(tab.linkedBrowser,
                       { tourId: TOUR_IDs[0], focusedId: TOUR_IDs[0] });

  for (let { key, options = {}, expected } of TOUR_LIST_TEST_DATA) {
    info(`Pressing ${key} to select ${expected.tourId} and have focus on ${expected.focusedId}`);
    await BrowserTestUtils.synthesizeKey(key, options, tab.linkedBrowser);
    await assertOverlayState(tab.linkedBrowser, expected);
  }

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_buttons_keyboard_navigation() {
  resetOnboardingDefaultState();

  info("Wait for onboarding overlay loaded");
  let tab = await openTab(ABOUT_HOME_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);

  info("Set keyboard focus on the onboarding overlay button");
  await ContentTask.spawn(tab.linkedBrowser, {}, () =>
    content.document.getElementById("onboarding-overlay-button").focus());
  await assertOverlayState(tab.linkedBrowser,
    { focusedId: "onboarding-overlay-button", visible: false });

  for (let { key, options = {}, expected } of BUTTONS_TEST_DATA) {
    info(`Pressing ${key} to have ${expected.visible ? "visible" : "invisible"} overlay and have focus on ${expected.focusedId}`);
    await BrowserTestUtils.synthesizeKey(key, options, tab.linkedBrowser);
    await assertOverlayState(tab.linkedBrowser, expected);
  }

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_overlay_dialog_keyboard_navigation() {
  resetOnboardingDefaultState();

  info("Wait for onboarding overlay loaded");
  let tab = await openTab(ABOUT_HOME_URL);
  let browser = tab.linkedBrowser;
  await promiseOnboardingOverlayLoaded(browser);

  info("Test accessibility and semantics of the dialog overlay");
  await assertModalDialog(browser, { visible: false });

  info("Set keyboard focus on the onboarding overlay button");
  await ContentTask.spawn(browser, {}, () =>
    content.document.getElementById("onboarding-overlay-button").focus());
  info("Open dialog with keyboard and check the dialog state");
  await BrowserTestUtils.synthesizeKey(" ", {}, browser);
  await promiseOnboardingOverlayOpened(browser);
  await assertModalDialog(browser,
    { visible: true, keyboardFocus: true, focusedId: TOUR_IDs[0] });

  info("Close the dialog and check modal dialog state");
  await BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, browser);
  await promiseOnboardingOverlayClosed(browser);
  await assertModalDialog(browser,
    { visible: false, keyboardFocus: true, focusedId: "onboarding-overlay-button" });

  BrowserTestUtils.removeTab(tab);
});
