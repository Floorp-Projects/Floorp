/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);
const TEST_PAGE = TEST_ROOT + "get_user_media.html";

/**
 * Tests that the given tab is the currently selected tab.
 * @param {Element} aTab - Tab to test.
 */
function testSelected(aTab) {
  is(aTab, gBrowser.selectedTab, "Tab is gBrowser.selectedTab");
  ok(aTab.hasAttribute("selected"), "Tab has attribute 'selected'");
  ok(
    aTab.hasAttribute("visuallyselected"),
    "Tab has attribute 'visuallyselected'"
  );
}

/**
 * Tests that when closing a tab with active screen sharing, the screen sharing
 * ends and the tab closes properly.
 */
add_task(async function testScreenSharingTabClose() {
  let initialTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );

  // Open another foreground tab and ensure its selected.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
  testSelected(tab);

  // Start screen sharing in active tab
  await shareDevices(tab.linkedBrowser, false, false, SHARE_WINDOW);
  ok(tab._sharingState.webRTC.screen, "Tab has webRTC screen sharing state");

  let recordingEndedPromise = expectObserverCalled(
    "recording-window-ended",
    1,
    tab.linkedBrowser.browsingContext
  );
  let tabClosedPromise = BrowserTestUtils.waitForCondition(
    () => gBrowser.selectedTab == initialTab,
    "Waiting for tab to close"
  );

  // Close tab
  BrowserTestUtils.removeTab(tab, { animate: true });

  // Wait for screen sharing to end
  await recordingEndedPromise;

  // Wait for tab to be fully closed
  await tabClosedPromise;

  // Test that we're back to the initial tab.
  testSelected(initialTab);

  // There should be no active sharing for the selected tab.
  ok(
    !gBrowser.selectedTab._sharingState?.webRTC?.screen,
    "Selected tab doesn't have webRTC screen sharing state"
  );

  BrowserTestUtils.removeTab(initialTab);
});
