/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

const TAB_PICKUP_STATE_PREF =
  "browser.tabs.firefox-view.ui-state.tab-pickup.open";
const RECENTLY_CLOSED_STATE_PREF =
  "browser.tabs.firefox-view.ui-state.recently-closed-tabs.open";

add_task(async function test_state_prefs_unset() {
  await SpecialPowers.clearUserPref(TAB_PICKUP_STATE_PREF);
  await SpecialPowers.clearUserPref(RECENTLY_CLOSED_STATE_PREF);

  const sandbox = sinon.createSandbox();
  let setupCompleteStub = sandbox.stub(
    TabsSetupFlowManager,
    "isTabSyncSetupComplete"
  );
  setupCompleteStub.returns(true);

  await withFirefoxView({}, async function(browser) {
    const { document } = browser.contentWindow;
    let recentlyClosedTabsContainer = document.querySelector(
      "#recently-closed-tabs-container"
    );
    ok(
      recentlyClosedTabsContainer.open,
      "Recently Closed Tabs should be open if the pref is unset and sync setup is complete"
    );

    let tabPickupContainer = document.querySelector("#tab-pickup-container");
    ok(
      tabPickupContainer.open,
      "Tab Pickup container should be open if the pref is unset and sync setup is complete"
    );

    sandbox.restore();
  });
});

add_task(async function test_state_prefs_defined() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [TAB_PICKUP_STATE_PREF, false],
      [RECENTLY_CLOSED_STATE_PREF, false],
    ],
  });

  const sandbox = sinon.createSandbox();
  let setupCompleteStub = sandbox.stub(
    TabsSetupFlowManager,
    "isTabSyncSetupComplete"
  );
  setupCompleteStub.returns(true);

  await withFirefoxView({}, async function(browser) {
    const { document } = browser.contentWindow;
    let recentlyClosedTabsContainer = document.querySelector(
      "#recently-closed-tabs-container"
    );
    ok(
      !recentlyClosedTabsContainer.getAttribute("open"),
      "Recently Closed Tabs should not be open if the pref is set to false"
    );

    let tabPickupContainer = document.querySelector("#tab-pickup-container");
    ok(
      !tabPickupContainer.getAttribute("open"),
      "Tab Pickup container should not be open if the pref is set to false and sync setup is complete"
    );

    sandbox.restore();
  });
});

add_task(async function test_state_pref_set_on_toggle() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [TAB_PICKUP_STATE_PREF, true],
      [RECENTLY_CLOSED_STATE_PREF, true],
    ],
  });

  const sandbox = sinon.createSandbox();
  let setupCompleteStub = sandbox.stub(
    TabsSetupFlowManager,
    "isTabSyncSetupComplete"
  );
  setupCompleteStub.returns(true);

  await withFirefoxView({}, async function(browser) {
    const { document } = browser.contentWindow;

    await waitForElementVisible(browser, "#tab-pickup-container > summary");

    document.querySelector("#tab-pickup-container > summary").click();

    document.querySelector("#recently-closed-tabs-container > summary").click();

    // Wait a turn for the click to propagate to the pref.
    await TestUtils.waitForTick();

    ok(
      !Services.prefs.getBoolPref(RECENTLY_CLOSED_STATE_PREF),
      "Hiding the recently closed container should have flipped the UI state pref value"
    );
    ok(
      !Services.prefs.getBoolPref(TAB_PICKUP_STATE_PREF),
      "Hiding the tab pickup container should have flipped the UI state pref value"
    );

    sandbox.restore();
  });
});

add_task(async function test_state_prefs_ignored_during_sync_setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [TAB_PICKUP_STATE_PREF, false],
      [RECENTLY_CLOSED_STATE_PREF, false],
    ],
  });
  const sandbox = sinon.createSandbox();
  let setupCompleteStub = sandbox.stub(
    TabsSetupFlowManager,
    "isTabSyncSetupComplete"
  );
  setupCompleteStub.returns(false);
  await withFirefoxView({}, async function(browser) {
    const { document } = browser.contentWindow;
    let recentlyClosedTabsContainer = document.querySelector(
      "#recently-closed-tabs-container"
    );
    ok(
      !recentlyClosedTabsContainer.open,
      "Recently Closed Tabs should not be open if the pref is set to false"
    );

    let tabPickupContainer = document.querySelector("#tab-pickup-container");
    ok(
      tabPickupContainer.open,
      "Tab Pickup container should be open if the pref is set to false but sync setup is not complete"
    );

    sandbox.restore();
  });
});
