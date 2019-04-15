/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests that typing a url and picking a switch to tab actually switches
 * to the right tab. Also tests repeated keydown/keyup events don't confuse
 * override.
 */

"use strict";

add_task(async function test_switchToTab_url() {
  const TEST_URL = "https://example.org/browser/";

  let baseTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let testTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  // Functions for TabClose and TabSelect
  let tabClosePromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer,
    "TabClose", false, event => event.target == testTab);
  let tabSelectPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer,
    "TabSelect", false, event => event.target == baseTab);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: TEST_URL,
    fireInputEvent: true,
  });
  // The first result is the heuristic, the second will be the switch to tab.
  await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);

  // Simulate a long press, on some platforms (Windows) it can generate multiple
  // keydown events.
  EventUtils.synthesizeKey("VK_SHIFT", { type: "keydown", repeat: 3 });
  EventUtils.synthesizeKey("VK_SHIFT", { type: "keyup" });

  // Pick the switch to tab result.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");

  await Promise.all([tabSelectPromise, tabClosePromise]);

  // Confirm that the selected tab is now the base tab
  Assert.equal(gBrowser.selectedTab, baseTab,
    "Should have switched to the correct tab");

  gBrowser.removeTab(baseTab);
});
