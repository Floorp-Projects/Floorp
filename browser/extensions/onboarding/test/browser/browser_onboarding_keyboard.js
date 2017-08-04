/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

function assertTourList(browser, args) {
  return ContentTask.spawn(browser, args, ({ tourId, focusedId }) => {
    let doc = content.document;
    let items = [...doc.querySelectorAll(".onboarding-tour-item")];
    items.forEach(item => is(item.getAttribute("aria-selected"),
      item.id === tourId ? "true" : "false",
      "Active item should have aria-selected set to true and inactive to false"));
    let focused = doc.getElementById(focusedId);
    is(focused, doc.activeElement, `Focus should be set on ${focusedId}`);
  });
}

const TEST_DATA = [
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[1], focusedId: TOUR_IDs[1] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[2], focusedId: TOUR_IDs[2] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[3] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[4], focusedId: TOUR_IDs[4] }},
  { key: "VK_DOWN", expected: { tourId: TOUR_IDs[5], focusedId: TOUR_IDs[5] }},
  { key: "VK_UP", expected: { tourId: TOUR_IDs[4], focusedId: TOUR_IDs[4] }},
  { key: "VK_UP", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[3] }},
  { key: "VK_TAB", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[4] }},
  { key: "VK_TAB", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[5] }},
  { key: "VK_RETURN", expected: { tourId: TOUR_IDs[5], focusedId: TOUR_IDs[5] }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { tourId: TOUR_IDs[5], focusedId: TOUR_IDs[4] }},
  { key: "VK_TAB", options: { shiftKey: true }, expected: { tourId: TOUR_IDs[5], focusedId: TOUR_IDs[3] }},
  // VK_SPACE does not work well with EventUtils#synthesizeKey use " " instead
  { key: " ", expected: { tourId: TOUR_IDs[3], focusedId: TOUR_IDs[3] }}
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
  await assertTourList(tab.linkedBrowser,
                       { tourId: TOUR_IDs[0], focusedId: TOUR_IDs[0] });

  for (let { key, options = {}, expected } of TEST_DATA) {
    info(`Pressing ${key} to select ${expected.tourId} and have focus on ${expected.focusedId}`);
    await BrowserTestUtils.synthesizeKey(key, options, tab.linkedBrowser);
    await assertTourList(tab.linkedBrowser, expected);
  }

  await BrowserTestUtils.removeTab(tab);
});
