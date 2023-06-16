/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(async function () {
  Services.prefs.clearUserPref(TAB_PICKUP_STATE_PREF);
});

async function setup({ open } = {}) {
  TabsSetupFlowManager.resetInternalState();
  // sanity check initial values
  ok(
    !TabsSetupFlowManager.hasVisibleViews,
    "Initially hasVisibleViews is false"
  );
  is(
    TabsSetupFlowManager._viewVisibilityStates.size,
    0,
    "Initially, there are no visible views"
  );
  ok(
    !isFirefoxViewTabSelected(),
    "During setup, the about:firefoxview tab is not selected"
  );

  if (typeof open == "undefined") {
    Services.prefs.clearUserPref(TAB_PICKUP_STATE_PREF);
  } else {
    await SpecialPowers.pushPrefEnv({
      set: [[TAB_PICKUP_STATE_PREF, open]],
    });
  }
  const sandbox = sinon.createSandbox();
  sandbox.stub(TabsSetupFlowManager, "isTabSyncSetupComplete").get(() => true);
  return sandbox;
}

add_task(async function test_tab_pickup_visibility() {
  /* Confirm the correct number of tab-pickup views are registered as visible */
  const sandbox = await setup();

  await withFirefoxView({ win: window }, async function (browser) {
    const { document } = browser.contentWindow;
    let tabPickupContainer = document.querySelector("#tab-pickup-container");

    ok(tabPickupContainer.open, "Tab Pickup container should be open");
    ok(isFirefoxViewTabSelected(), "The firefox view tab is selected");
    ok(TabsSetupFlowManager.hasVisibleViews, "hasVisibleViews");
    is(TabsSetupFlowManager._viewVisibilityStates.size, 1, "One view");

    info("Opening and switching to different tab to background fx-view");
    let newTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:mozilla"
    );
    ok(!isFirefoxViewTabSelected(), "The firefox view tab is not selected");
    ok(
      !TabsSetupFlowManager.hasVisibleViews,
      "no view visible when fx-view is not active"
    );
    let newWin = await BrowserTestUtils.openNewBrowserWindow();
    await openFirefoxViewTab(newWin);

    ok(
      isFirefoxViewTabSelected(newWin),
      "The firefox view tab in the new window is selected"
    );
    ok(
      TabsSetupFlowManager.hasVisibleViews,
      "view registered as visible when fx-view is opened in a new window"
    );
    is(TabsSetupFlowManager._viewVisibilityStates.size, 2, "2 tracked views");

    await BrowserTestUtils.closeWindow(newWin);

    ok(
      !isFirefoxViewTabSelected(),
      "The firefox view tab in the original window is not selected"
    );
    ok(
      !TabsSetupFlowManager.hasVisibleViews,
      "no visible views when fx-view is not the active tab in the remaining window"
    );
    is(
      TabsSetupFlowManager._viewVisibilityStates.size,
      1,
      "Back to one tracked view"
    );

    // Switch back to FxView:
    await BrowserTestUtils.switchTab(
      gBrowser,
      gBrowser.getTabForBrowser(browser)
    );

    ok(
      isFirefoxViewTabSelected(),
      "The firefox view tab in the original window is now selected"
    );
    ok(
      TabsSetupFlowManager.hasVisibleViews,
      "View visibility updated when we switch tab"
    );
    BrowserTestUtils.removeTab(newTab);
  });
  sandbox.restore();
  await SpecialPowers.popPrefEnv();
  ok(
    !TabsSetupFlowManager.hasVisibleViews,
    "View visibility updated after withFirefoxView"
  );
});

add_task(async function test_instance_closed() {
  /* Confirm tab-pickup views are correctly accounted for when toggled closed */
  const sandbox = await setup({ open: false });
  await withFirefoxView({ win: window }, async function (browser) {
    const { document } = browser.contentWindow;
    info(
      "tab-pickup.open pref: " +
        Services.prefs.getBoolPref(
          "browser.tabs.firefox-view.ui-state.tab-pickup.open"
        )
    );
    info(
      "isTabSyncSetupComplete: " + TabsSetupFlowManager.isTabSyncSetupComplete
    );
    let tabPickupContainer = document.querySelector("#tab-pickup-container");
    ok(!tabPickupContainer.open, "Tab Pickup container should be closed");
    info(
      "_viewVisibilityStates" +
        JSON.stringify(
          Array.from(TabsSetupFlowManager._viewVisibilityStates.values()),
          null,
          2
        )
    );
    ok(!TabsSetupFlowManager.hasVisibleViews, "no visible views");
    is(
      TabsSetupFlowManager._viewVisibilityStates.size,
      1,
      "One registered view"
    );

    tabPickupContainer.open = true;
    await TestUtils.waitForTick();
    ok(TabsSetupFlowManager.hasVisibleViews, "view visible");
  });
  sandbox.restore();
});
