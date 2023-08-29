/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that are related to the accessibility of the Firefox View
 * document. These tasks tend to be privileged content, not browser
 * chrome.
 */

add_setup(async function setup() {
  // Make sure the prompt to connect FxA doesn't show
  // Without resetting the view-count pref it gets surfaced after
  // the third click on the fx view toolbar button.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.firefox-view.view-count", 0],
      ["browser.tabs.firefox-view-next", false],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_keyboard_focus_after_tab_pickup_opened() {
  // Reset various things touched by other tests in this file so that
  // we have a sufficiently clean environment.

  TabsSetupFlowManager.resetInternalState();

  // Ensure that the tab-pickup section doesn't need to be opened.
  Services.prefs.clearUserPref(
    "browser.tabs.firefox-view.ui-state.tab-pickup.open"
  );

  // Let's be deterministic about the basic UI state!
  const sandbox = setupMocks({
    state: UIState.STATUS_NOT_CONFIGURED,
    syncEnabled: false,
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;

    is(
      document.activeElement.localName,
      "body",
      "document body element is initially focused"
    );

    const tab = () => {
      info("Tab keypress synthesized");
      EventUtils.synthesizeKey("KEY_Tab", {}, win);
    };

    tab();

    let tabPickupContainer = document.querySelector(
      "#tab-pickup-container summary.page-section-header"
    );
    is(
      document.activeElement,
      tabPickupContainer,
      "tab pickup container header has focus"
    );

    tab();

    is(
      document.activeElement.id,
      "firefoxview-tabpickup-step-signin-primarybutton",
      "tab pickup primary button has focus"
    );
  });

  // cleanup time
  await tearDown(sandbox);
});

add_task(async function test_keyboard_accessibility_tab_pickup() {
  await withFirefoxView({}, async browser => {
    const win = browser.ownerGlobal;
    const { document } = browser.contentWindow;
    const enter = async () => {
      info("Enter");
      EventUtils.synthesizeKey("KEY_Enter", {}, win);
    };
    let details = document.getElementById("tab-pickup-container");
    let summary = details.querySelector("summary");
    ok(summary, "summary element should exist");
    ok(details.open, "Tab pickup container should be initially open on load");
    summary.focus();
    await enter();
    ok(!details.open, "Tab pickup container should be closed");
    await enter();
    ok(details.open, "Tab pickup container should be opened");
  });
  cleanup_tab_pickup();
});
