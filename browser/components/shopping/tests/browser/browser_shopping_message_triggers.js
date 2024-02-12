/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);

const { FeatureCalloutMessages } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/FeatureCalloutMessages.sys.mjs"
);

const OPTED_IN_PREF = "browser.shopping.experience2023.optedIn";
const ACTIVE_PREF = "browser.shopping.experience2023.active";
const CFR_ENABLED_PREF =
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features";

const CONTENT_PAGE = "https://example.com";
const PRODUCT_PAGE = "https://example.com/product/B09TJGHL5F";

add_setup(async function setup() {
  // disable auto-activation to prevent interference with the tests
  ShoppingUtils.handledAutoActivate = true;
  // clean up all the prefs/states modified by this test
  registerCleanupFunction(() => {
    ShoppingUtils.handledAutoActivate = false;
  });
});

/** Test that the correct callouts show for opted-in users */
add_task(async function test_fakespot_callouts_opted_in_flow() {
  // Re-enable feature callouts for this test. This has to be done in each task
  // because they're disabled in browser.ini.
  await SpecialPowers.pushPrefEnv({ set: [[CFR_ENABLED_PREF, true]] });
  let sandbox = sinon.createSandbox();
  let routeCFRMessageStub = sandbox
    .stub(ASRouter, "routeCFRMessage")
    .withArgs(
      sinon.match.any,
      sinon.match.any,
      sinon.match({ id: "shoppingProductPageWithSidebarClosed" })
    );
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );

  // Reset opt-in but make the sidebar active so it appears on PDP.
  await SpecialPowers.pushPrefEnv({
    set: [
      [ACTIVE_PREF, true],
      [OPTED_IN_PREF, 0],
    ],
  });

  // Visit a product page and wait for the sidebar to open.
  let pdpTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PRODUCT_PAGE
  );
  let pdpBrowser = pdpTab.linkedBrowser;
  let pdpBrowserPanel = gBrowser.getPanel(pdpBrowser);
  let isSidebarVisible = () => {
    let sidebar = pdpBrowserPanel.querySelector("shopping-sidebar");
    return sidebar && BrowserTestUtils.isVisible(sidebar);
  };
  await BrowserTestUtils.waitForMutationCondition(
    pdpBrowserPanel,
    { childList: true, attributeFilter: ["hidden"] },
    isSidebarVisible
  );
  ok(isSidebarVisible(), "Shopping sidebar should be open on a product page");

  // Visiting the PDP should not cause shoppingProductPageWithSidebarClosed to
  // fire in this case, because the sidebar is active.
  ok(
    routeCFRMessageStub.neverCalledWithMatch(
      sinon.match.any,
      sinon.match.any,
      sinon.match({ id: "shoppingProductPageWithSidebarClosed" })
    ),
    "shoppingProductPageWithSidebarClosed should not fire when sidebar is active"
  );

  // Now opt in...
  let prefChanged = TestUtils.waitForPrefChange(
    OPTED_IN_PREF,
    value => value === 1
  );
  await SpecialPowers.pushPrefEnv({ set: [[OPTED_IN_PREF, 1]] });
  await prefChanged;

  // Close the sidebar by deactivating the global toggle, and wait for the
  // shoppingProductPageWithSidebarClosed trigger to fire.
  let shoppingProductPageWithSidebarClosedMsg = new Promise(resolve => {
    routeCFRMessageStub.callsFake((message, browser, trigger) => {
      if (
        trigger.id === "shoppingProductPageWithSidebarClosed" &&
        trigger.context.isSidebarClosing
      ) {
        resolve(message?.id);
      }
    });
  });
  await SpecialPowers.pushPrefEnv({ set: [[ACTIVE_PREF, false]] });
  // Assert that the message is the one we expect.
  is(
    await shoppingProductPageWithSidebarClosedMsg,
    "FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT",
    "Should route the expected message: FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT"
  );
  BrowserTestUtils.removeTab(pdpTab);

  // Now, having seen the on-closed callout, we should expect to see the on-PDP
  // callout on the next PDP visit, provided it's been at least 24 hours.
  //
  // Of course we can't really do that in an automated test, so we'll override
  // the message impression date to simulate that.
  //
  // But first, try opening a PDP so we can test that it _doesn't_ fire if less
  // than 24hrs has passed.

  // Visit a product page and wait for routeCFRMessage to fire, expecting the
  // message to be null due to targeting.
  shoppingProductPageWithSidebarClosedMsg = new Promise(resolve => {
    routeCFRMessageStub.callsFake((message, browser, trigger) => {
      if (
        trigger.id === "shoppingProductPageWithSidebarClosed" &&
        !trigger.context.isSidebarClosing
      ) {
        resolve(message?.id);
      }
    });
  });
  pdpTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PRODUCT_PAGE);
  // Assert that the on-PDP message is not matched, due to targeting.
  isnot(
    await shoppingProductPageWithSidebarClosedMsg,
    "FAKESPOT_CALLOUT_PDP_OPTED_IN_DEFAULT",
    "Should not route the on-PDP message because the on-close message was seen recently"
  );
  BrowserTestUtils.removeTab(pdpTab);

  // Now override the state so it looks like we closed the sidebar 25 hours ago.
  let lastClosedDate = Date.now() - 25 * 60 * 60 * 1000; // 25 hours ago
  await ASRouter.setState(state => {
    const messageImpressions = { ...state.messageImpressions };
    messageImpressions.FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT = [
      lastClosedDate,
    ];
    ASRouter._storage.set("messageImpressions", messageImpressions);
    return { messageImpressions };
  });

  // And open a new PDP, expecting the on-PDP message to be routed.
  shoppingProductPageWithSidebarClosedMsg = new Promise(resolve => {
    routeCFRMessageStub.callsFake((message, browser, trigger) => {
      if (
        trigger.id === "shoppingProductPageWithSidebarClosed" &&
        !trigger.context.isSidebarClosing
      ) {
        resolve(message?.id);
      }
    });
  });
  pdpTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PRODUCT_PAGE);
  // Assert that the on-PDP message is now matched, due to targeting.
  is(
    await shoppingProductPageWithSidebarClosedMsg,
    "FAKESPOT_CALLOUT_PDP_OPTED_IN_DEFAULT",
    "Should route the on-PDP message"
  );
  BrowserTestUtils.removeTab(pdpTab);

  // Clean up.
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await ASRouter.setState(state => {
    const messageImpressions = { ...state.messageImpressions };
    delete messageImpressions.FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT;
    ASRouter._storage.set("messageImpressions", messageImpressions);
    return { messageImpressions };
  });
  sandbox.restore();
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );
});

/** Test that the correct callouts show for not-opted-in users */
add_task(async function test_fakespot_callouts_not_opted_in_flow() {
  await SpecialPowers.pushPrefEnv({ set: [[CFR_ENABLED_PREF, true]] });
  let sandbox = sinon.createSandbox();
  let routeCFRMessageStub = sandbox
    .stub(ASRouter, "routeCFRMessage")
    .withArgs(
      sinon.match.any,
      sinon.match.any,
      sinon.match({ id: "shoppingProductPageWithSidebarClosed" })
    );
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );

  // Reset opt-in but make the sidebar active so it appears on PDP.
  await SpecialPowers.pushPrefEnv({
    set: [
      [ACTIVE_PREF, true],
      [OPTED_IN_PREF, 0],
    ],
  });

  // Visit a product page and wait for the sidebar to open.
  let pdpTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PRODUCT_PAGE
  );
  let pdpBrowser = pdpTab.linkedBrowser;
  let pdpBrowserPanel = gBrowser.getPanel(pdpBrowser);
  let isSidebarVisible = () => {
    let sidebar = pdpBrowserPanel.querySelector("shopping-sidebar");
    return sidebar && BrowserTestUtils.isVisible(sidebar);
  };
  await BrowserTestUtils.waitForMutationCondition(
    pdpBrowserPanel,
    { childList: true, attributeFilter: ["hidden"] },
    isSidebarVisible
  );
  ok(isSidebarVisible(), "Shopping sidebar should be open on a product page");

  // Close the sidebar by deactivating the global toggle, and wait for the
  // shoppingProductPageWithSidebarClosed trigger to fire.
  let shoppingProductPageWithSidebarClosedMsg = new Promise(resolve => {
    routeCFRMessageStub.callsFake((message, browser, trigger) => {
      if (
        trigger.id === "shoppingProductPageWithSidebarClosed" &&
        trigger.context.isSidebarClosing
      ) {
        resolve(message?.id);
      }
    });
  });
  await SpecialPowers.pushPrefEnv({ set: [[ACTIVE_PREF, false]] });
  // Assert that the message is the one we expect.
  is(
    await shoppingProductPageWithSidebarClosedMsg,
    "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT",
    "Should route the expected message: FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT"
  );
  BrowserTestUtils.removeTab(pdpTab);

  // Unlike the opted-in flow, at this point we should not expect to see any
  // more callouts, because the flow ends after the on-closed callout. So we can
  // test that FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT's targeting excludes us
  // even if it's been 25 hours since the sidebar was closed.

  // As with the opted-in flow, override the state so it looks like we closed
  // the sidebar 25 hours ago.
  let lastClosedDate = Date.now() - 25 * 60 * 60 * 1000; // 25 hours ago
  await ASRouter.setState(state => {
    const messageImpressions = { ...state.messageImpressions };
    messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT = [
      lastClosedDate,
    ];
    ASRouter._storage.set("messageImpressions", messageImpressions);
    return { messageImpressions };
  });

  // Visit a product page and wait for routeCFRMessage to fire, expecting the
  // message to be null due to targeting.
  shoppingProductPageWithSidebarClosedMsg = new Promise(resolve => {
    routeCFRMessageStub.callsFake((message, browser, trigger) => {
      if (
        trigger.id === "shoppingProductPageWithSidebarClosed" &&
        !trigger.context.isSidebarClosing
      ) {
        resolve(message?.id);
      }
    });
  });
  pdpTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PRODUCT_PAGE);
  // Assert that the on-PDP message is not matched, due to targeting.
  isnot(
    await shoppingProductPageWithSidebarClosedMsg,
    "FAKESPOT_CALLOUT_PDP_OPTED_IN_DEFAULT",
    "Should not route the on-PDP message because the user is not opted in"
  );
  BrowserTestUtils.removeTab(pdpTab);

  // Clean up. We don't need to verify that the frequency caps work, since
  // that's a generic ASRouter feature.
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
  await ASRouter.setState(state => {
    const messageImpressions = { ...state.messageImpressions };
    delete messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT;
    ASRouter._storage.set("messageImpressions", messageImpressions);
    return { messageImpressions };
  });
  sandbox.restore();
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );
});
