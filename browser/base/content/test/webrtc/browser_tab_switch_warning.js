/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the warning that is displayed when switching to background
 * tabs when sharing the browser window or screen
 */

// The number of tabs to have in the background for testing.
const NEW_BACKGROUND_TABS_TO_OPEN = 5;
const WARNING_PANEL_ID = "sharing-tabs-warning-panel";
const ALLOW_BUTTON_ID = "sharing-warning-proceed-to-tab";
const DISABLE_WARNING_FOR_SESSION_CHECKBOX_ID =
  "sharing-warning-disable-for-session";
const WINDOW_SHARING_HEADER_ID = "sharing-warning-window-panel-header";
const SCREEN_SHARING_HEADER_ID = "sharing-warning-screen-panel-header";
// The number of milliseconds we're willing to wait for the
// warning panel before we decide that it's not coming.
const WARNING_PANEL_TIMEOUT_MS = 1000;
const CTRL_TAB_RUO_PREF = "browser.ctrlTab.sortByRecentlyUsed";

/**
 * Common helper function that pretendToShareWindow and pretendToShareScreen
 * call into. Ensures that the first tab is selected, and then (optionally)
 * does the first "freebie" tab switch to the second tab.
 *
 * @param {boolean} doFirstTabSwitch - True if this function should take
 *   care of doing the "freebie" tab switch for you.
 * @return {Promise}
 * @resolves {undefined} - Once the simulation is set up.
 */
async function pretendToShareDisplay(doFirstTabSwitch) {
  Assert.equal(
    gBrowser.selectedTab,
    gBrowser.tabs[0],
    "Should start on the first tab."
  );

  webrtcUI.sharingDisplay = true;
  if (doFirstTabSwitch) {
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[1]);
  }
}

/**
 * Simulates the sharing of a particular browser window. The
 * simulation doesn't actually share the window over WebRTC, but
 * does enough to convince webrtcUI that the window is in the shared
 * window list.
 *
 * It is assumed that the first tab is the selected tab when calling
 * this function.
 *
 * This helper function can also automatically perform the first
 * "freebie" tab switch that never warns. This is its default behaviour.
 *
 * @param {DOM Window} aWindow - The window that we're simulating sharing.
 * @param {boolean} doFirstTabSwitch - True if this function should take
 *   care of doing the "freebie" tab switch for you. Defaults to true.
 * @return {Promise}
 * @resolves {undefined} - Once the simulation is set up.
 */
async function pretendToShareWindow(aWindow, doFirstTabSwitch = true) {
  // Poke into webrtcUI so that it thinks that the current browser
  // window is being shared.
  webrtcUI.sharedBrowserWindows.add(aWindow);
  await pretendToShareDisplay(doFirstTabSwitch);
}

/**
 * Simulates the sharing of the screen. The simulation doesn't actually share
 * the screen over WebRTC, but does enough to convince webrtcUI that the screen
 * is being shared.
 *
 * It is assumed that the first tab is the selected tab when calling
 * this function.
 *
 * This helper function can also automatically perform the first
 * "freebie" tab switch that never warns. This is its default behaviour.
 *
 * @param {boolean} doFirstTabSwitch - True if this function should take
 *   care of doing the "freebie" tab switch for you. Defaults to true.
 * @return {Promise}
 * @resolves {undefined} - Once the simulation is set up.
 */
async function pretendToShareScreen(doFirstTabSwitch = true) {
  // Poke into webrtcUI so that it thinks that the current screen is being
  // shared.
  webrtcUI.sharingScreen = true;
  await pretendToShareDisplay(doFirstTabSwitch);
}

/**
 * Resets webrtcUI's notion of what is being shared. This also clears
 * out any simulated shared windows, and resets any state that only
 * persists for a sharing session.
 *
 * This helper function will also:
 * 1. Switch back to the first tab if it's not already selected.
 * 2. Check if the tab switch warning panel is open, and if so, close it.
 *
 * @return {Promise}
 * @resolves {undefined} - Once the state is reset.
 */
async function resetDisplaySharingState() {
  let firstTabBC = gBrowser.browsers[0].browsingContext;
  webrtcUI.streamAddedOrRemoved(firstTabBC, { remove: true });

  if (gBrowser.selectedTab !== gBrowser.tabs[0]) {
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  }

  let panel = document.getElementById(WARNING_PANEL_ID);
  if (panel && (panel.state == "open" || panel.state == "showing")) {
    info("Closing the warning panel.");
    let panelHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
    panel.hidePopup();
    await panelHidden;
  }
}

/**
 * Checks to make sure that a tab switch warning doesn't show
 * within WARNING_PANEL_TIMEOUT_MS milliseconds.
 *
 * @return {Promise}
 * @resolves {undefined} - Once the check is complete.
 */
async function ensureNoWarning() {
  let timerExpired = false;
  let sawWarning = false;

  let resolver;
  let timeoutOrPopupShowingPromise = new Promise(resolve => {
    resolver = resolve;
  });

  let onPopupShowing = event => {
    if (event.target.id == WARNING_PANEL_ID) {
      sawWarning = true;
      resolver();
    }
  };
  // The panel might not have been lazily-inserted yet, so we
  // attach the popupshowing handler to the window instead.
  window.addEventListener("popupshowing", onPopupShowing);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  let timer = setTimeout(() => {
    timerExpired = true;
    resolver();
  }, WARNING_PANEL_TIMEOUT_MS);

  await timeoutOrPopupShowingPromise;

  clearTimeout(timer);
  window.removeEventListener("popupshowing", onPopupShowing);

  Assert.ok(timerExpired, "Timer should have expired.");
  Assert.ok(!sawWarning, "Should not have shown the tab switch warning.");
}

/**
 * Checks to make sure that a tab switch warning appears for
 * a particular tab.
 *
 * @param {<xul:tab>} tab - The tab that the warning should be anchored to.
 * @return {Promise}
 * @resolves {undefined} - Once the check is complete.
 */
async function ensureWarning(tab) {
  let popupShowingEvent = await BrowserTestUtils.waitForEvent(
    window,
    "popupshowing",
    false,
    event => {
      return event.target.id == WARNING_PANEL_ID;
    }
  );
  let panel = popupShowingEvent.target;

  Assert.equal(
    panel.anchorNode,
    tab,
    "Expected the warning to be anchored to the right tab."
  );
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.webrtc.sharedTabWarning", true]],
  });

  // Loads up NEW_BACKGROUND_TABS_TO_OPEN background tabs at about:blank,
  // and waits until they're fully open.
  let uris = new Array(NEW_BACKGROUND_TABS_TO_OPEN).fill("about:blank");

  let loadPromises = Promise.all(
    uris.map(uri => BrowserTestUtils.waitForNewTab(gBrowser, uri, false, true))
  );

  gBrowser.loadTabs(uris, {
    inBackground: true,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });

  await loadPromises;

  // Switches to the first tab and closes all of the rest.
  registerCleanupFunction(async () => {
    await resetDisplaySharingState();
    gBrowser.removeAllTabsBut(gBrowser.tabs[0]);
  });
});

/**
 * Tests that when sharing the window that the first tab switch does _not_ show
 * the warning. This is because we presume that the first tab switch since
 * starting display sharing is for a tab that is intentionally being shared.
 */
add_task(async function testFirstTabSwitchAllowed() {
  pretendToShareWindow(window, false);

  let targetTab = gBrowser.tabs[1];

  let noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await noWarningPromise;

  await resetDisplaySharingState();
});

/**
 * Tests that the second tab switch after sharing is not allowed
 * without a warning. Also tests that the warning can "allow"
 * the tab switch to proceed, and that no warning is subsequently
 * shown for the "allowed" tab. Finally, ensures that if the sharing
 * session ends and a new session begins, that warnings are shown
 * again for the allowed tabs.
 */
add_task(async function testWarningOnSecondTabSwitch() {
  pretendToShareWindow(window);
  let originalTab = gBrowser.selectedTab;

  // pretendToShareWindow will have switched us to the second
  // tab automatically as the first "freebie" tab switch

  let targetTab = gBrowser.tabs[2];

  // Ensure that we show the warning on the second tab switch
  let warningPromise = ensureWarning(targetTab);
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await warningPromise;

  // Not only should we have warned, but we should have prevented
  // the tab switch from occurring.
  Assert.equal(
    gBrowser.selectedTab,
    originalTab,
    "Should still be on the original tab."
  );

  // Now test the "Allow" button in the warning to make sure the tab
  // switch goes through.
  let tabSwitchPromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "TabSwitchDone"
  );
  let allowButton = document.getElementById(ALLOW_BUTTON_ID);
  allowButton.click();
  await tabSwitchPromise;

  Assert.equal(
    gBrowser.selectedTab,
    targetTab,
    "Should have switched tabs to the target."
  );

  // We shouldn't see a warning when switching back to that first
  // "freebie" tab.
  let noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, originalTab);
  await noWarningPromise;

  Assert.equal(
    gBrowser.selectedTab,
    originalTab,
    "Should have switched tabs back to the original tab."
  );

  // We shouldn't see a warning when switching back to the tab that
  // we had just allowed.
  noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await noWarningPromise;

  Assert.equal(
    gBrowser.selectedTab,
    targetTab,
    "Should have switched tabs back to the target tab."
  );

  // Reset the sharing state, and make sure that warnings can
  // be displayed again.
  await resetDisplaySharingState();
  pretendToShareWindow(window);

  // pretendToShareWindow will have switched us to the second
  // tab automatically as the first "freebie" tab switch
  //
  // Make sure we get the warning again when switching to the
  // target tab.
  warningPromise = ensureWarning(targetTab);
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await warningPromise;

  await resetDisplaySharingState();
});

/**
 * Tests that warnings can be skipped for a session via the
 * checkbox in the warning panel. Also checks that once the
 * session ends and a new one begins that warnings are displayed
 * again.
 */
add_task(async function testDisableWarningForSession() {
  pretendToShareWindow(window);

  // pretendToShareWindow will have switched us to the second
  // tab automatically as the first "freebie" tab switch
  let targetTab = gBrowser.tabs[2];

  // Ensure that we show the warning on the second tab switch
  let warningPromise = ensureWarning(targetTab);
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await warningPromise;

  // Check the checkbox to suppress warnings for the rest of this session.
  let checkbox = document.getElementById(
    DISABLE_WARNING_FOR_SESSION_CHECKBOX_ID
  );
  checkbox.checked = true;

  // Now test the "Allow" button in the warning to make sure the tab
  // switch goes through.
  let tabSwitchPromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "TabSwitchDone"
  );
  let allowButton = document.getElementById(ALLOW_BUTTON_ID);
  allowButton.click();
  await tabSwitchPromise;

  Assert.equal(
    gBrowser.selectedTab,
    targetTab,
    "Should have switched tabs to the target tab."
  );

  // Switching to the 4th and 5th tabs should now not show warnings.
  let noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[3]);
  await noWarningPromise;

  noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[4]);
  await noWarningPromise;

  // Reset the sharing state, and make sure that warnings can
  // be displayed again.
  await resetDisplaySharingState();
  pretendToShareWindow(window);

  // pretendToShareWindow will have switched us to the second
  // tab automatically as the first "freebie" tab switch
  //
  // Make sure we get the warning again when switching to the
  // target tab.
  warningPromise = ensureWarning(targetTab);
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await warningPromise;

  await resetDisplaySharingState();
});

/**
 * Tests that we don't show a warning when sharing a different
 * window than the one we're switching tabs in.
 */
add_task(async function testOtherWindow() {
  let otherWin = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(window);
  pretendToShareWindow(otherWin);

  // Switching to the 4th and 5th tabs should now not show warnings.
  let noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[3]);
  await noWarningPromise;

  noWarningPromise = ensureNoWarning();
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[4]);
  await noWarningPromise;

  await BrowserTestUtils.closeWindow(otherWin);

  await resetDisplaySharingState();
});

/**
 * Tests that we show a different label when sharing the screen
 * vs when sharing a window.
 */
add_task(async function testWindowVsScreenLabel() {
  pretendToShareWindow(window);

  // pretendToShareWindow will have switched us to the second
  // tab automatically as the first "freebie" tab switch.
  // Let's now switch to the third tab.
  let targetTab = gBrowser.tabs[2];

  // Ensure that we show the warning on this second tab switch
  let warningPromise = ensureWarning(targetTab);
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await warningPromise;

  let windowHeader = document.getElementById(WINDOW_SHARING_HEADER_ID);
  let screenHeader = document.getElementById(SCREEN_SHARING_HEADER_ID);
  Assert.ok(
    !BrowserTestUtils.isHidden(windowHeader),
    "Should be showing window sharing header"
  );
  Assert.ok(
    BrowserTestUtils.isHidden(screenHeader),
    "Should not be showing screen sharing header"
  );

  // Reset the sharing state, and then pretend to share the screen.
  await resetDisplaySharingState();
  pretendToShareScreen();

  // Ensure that we show the warning on this second tab switch
  warningPromise = ensureWarning(targetTab);
  await BrowserTestUtils.switchTab(gBrowser, targetTab);
  await warningPromise;

  Assert.ok(
    BrowserTestUtils.isHidden(windowHeader),
    "Should not be showing window sharing header"
  );
  Assert.ok(
    !BrowserTestUtils.isHidden(screenHeader),
    "Should be showing screen sharing header"
  );
  await resetDisplaySharingState();
});

/**
 * Tests that tab switching via the keyboard can also trigger the
 * tab switch warnings.
 */
add_task(async function testKeyboardTabSwitching() {
  let pressCtrlTab = async (expectPanel = false) => {
    let promise;
    if (expectPanel) {
      promise = BrowserTestUtils.waitForEvent(ctrlTab.panel, "popupshown");
    } else {
      promise = BrowserTestUtils.waitForEvent(document, "keyup");
    }
    EventUtils.synthesizeKey("VK_TAB", {
      ctrlKey: true,
      shiftKey: false,
    });
    await promise;
  };

  let releaseCtrl = async () => {
    let promise;
    if (ctrlTab.isOpen) {
      promise = BrowserTestUtils.waitForEvent(ctrlTab.panel, "popuphidden");
    } else {
      promise = BrowserTestUtils.waitForEvent(document, "keyup");
    }
    EventUtils.synthesizeKey("VK_CONTROL", { type: "keyup" });
    return promise;
  };

  // Ensure that the (on by default) ctrl-tab switch panel is enabled.
  await SpecialPowers.pushPrefEnv({
    set: [[CTRL_TAB_RUO_PREF, true]],
  });

  pretendToShareWindow(window);
  let originalTab = gBrowser.selectedTab;
  await pressCtrlTab(true);

  // The Ctrl-Tab MRU list should be:
  // 0: Second tab (currently selected)
  // 1: First tab
  // 2: Last tab
  //
  // Having pressed Ctrl-Tab once, 1 (First tab) is selected in the
  // panel. We want a tab that will warn, so let's hit Ctrl-Tab again
  // to choose 2 (Last tab).
  let targetTab = ctrlTab.tabList[2];
  await pressCtrlTab();

  let warningPromise = ensureWarning(targetTab);
  await releaseCtrl();
  await warningPromise;

  // Hide the warning without allowing the tab switch.
  let panel = document.getElementById(WARNING_PANEL_ID);
  panel.hidePopup();

  Assert.equal(
    gBrowser.selectedTab,
    originalTab,
    "Should not have changed from the original tab."
  );

  // Now switch to the in-order tab switching keyboard shortcut mode.
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [[CTRL_TAB_RUO_PREF, false]],
  });

  // Hitting Ctrl-Tab should choose the _next_ tab over from
  // the originalTab, which should be the third tab.
  targetTab = gBrowser.tabs[2];

  warningPromise = ensureWarning(targetTab);
  await pressCtrlTab();
  await warningPromise;

  await resetDisplaySharingState();
});
