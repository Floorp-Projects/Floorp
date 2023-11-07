/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);

/**
 * Toggle prefs involved in automatically activating the sidebar on PDPs if the
 * user has not opted in. Onboarding should only try to auto-activate the
 * sidebar for non-opted-in users once per session at most, no more than once
 * per day, and no more than two times total.
 *
 * @param {object} states An object containing pref states to set. Leave a
 *   property undefined to ignore it.
 * @param {boolean} [states.active] Global sidebar toggle
 * @param {number} [states.optedIn] 2: opted out, 1: opted in, 0: not opted in
 * @param {number} [states.lastAutoActivate] Last auto activate date in seconds
 * @param {number} [states.autoActivateCount] Number of auto-activations (max 2)
 * @param {boolean} [states.handledAutoActivate] True if the sidebar handled its
 *   auto-activation logic this session, preventing further auto-activations
 */
function setOnboardingPrefs(states = {}) {
  if (Object.hasOwn(states, "handledAutoActivate")) {
    ShoppingUtils.handledAutoActivate = !!states.handledAutoActivate;
  }

  if (Object.hasOwn(states, "lastAutoActivate")) {
    Services.prefs.setIntPref(
      "browser.shopping.experience2023.lastAutoActivate",
      states.lastAutoActivate
    );
  }

  if (Object.hasOwn(states, "autoActivateCount")) {
    Services.prefs.setIntPref(
      "browser.shopping.experience2023.autoActivateCount",
      states.autoActivateCount
    );
  }

  if (Object.hasOwn(states, "optedIn")) {
    Services.prefs.setIntPref(
      "browser.shopping.experience2023.optedIn",
      states.optedIn
    );
  }

  if (Object.hasOwn(states, "active")) {
    Services.prefs.setBoolPref(
      "browser.shopping.experience2023.active",
      states.active
    );
  }

  if (Object.hasOwn(states, "telemetryEnabled")) {
    Services.prefs.setBoolPref(
      "browser.newtabpage.activity-stream.telemetry",
      states.telemetryEnabled
    );
  }
}

add_setup(async function setup() {
  // Block on testFlushAllChildren to ensure Glean is initialized before
  // running tests.
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  // Set all the prefs/states modified by this test to default values.
  registerCleanupFunction(() =>
    setOnboardingPrefs({
      active: true,
      optedIn: 1,
      lastAutoActivate: 0,
      autoActivateCount: 0,
      handledAutoActivate: false,
      telementryEnabled: false,
    })
  );
});

/**
 * Test to check onboarding message container is rendered
 * when user is not opted-in
 */
add_task(async function test_showOnboarding_notOptedIn() {
  // OptedIn pref Value is 0 when a user hasn't opted-in
  setOnboardingPrefs({ active: false, optedIn: 0, telemetryEnabled: true });

  Services.fog.testResetFOG();
  await Services.fog.testFlushAllChildren();

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      // Get the actor to update the product URL, since no content will render without one
      let actor =
        gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getExistingActor(
          "ShoppingSidebar"
        );
      actor.updateProductURL("https://example.com/product/B09TJGHL5F");

      await SpecialPowers.spawn(browser, [], async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("shopping-container"),
          "shopping-container"
        );

        let containerElem =
          shoppingContainer.shadowRoot.getElementById("shopping-container");
        let messageSlot = containerElem.getElementsByTagName("slot");

        // Check multi-stage-message-slot used to show opt-In message is
        // rendered inside shopping container when user optedIn pref value is 0
        ok(messageSlot.length, `message slot element exists`);
        is(
          messageSlot[0].name,
          "multi-stage-message-slot",
          "multi-stage-message-slot showing opt-in message rendered"
        );

        ok(
          !content.document.getElementById("multi-stage-message-root").hidden,
          "message is shown"
        );
      });
    }
  );

  if (!AppConstants.platform != "linux") {
    await Services.fog.testFlushAllChildren();
    const events = Glean.shopping.surfaceOnboardingDisplayed.testGetValue();

    if (events) {
      Assert.greater(events.length, 0);
      Assert.equal(events[0].category, "shopping");
      Assert.equal(events[0].name, "surface_onboarding_displayed");
    } else {
      info("Failed to get Glean value due to unknown bug. See bug 1862389.");
    }
  }
});

/**
 * Test to check onboarding message is not shown for opted-in users
 */
add_task(async function test_hideOnboarding_optedIn() {
  // OptedIn pref value is 1 for opted-in users
  setOnboardingPrefs({ active: false, optedIn: 1 });
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      // Get the actor to update the product URL, since no content will render without one
      let actor =
        gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getExistingActor(
          "ShoppingSidebar"
        );
      actor.updateProductURL("https://example.com/product/B09TJGHL5F");

      await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("shopping-container"),
          "shopping-container"
        );

        ok(
          content.document.getElementById("multi-stage-message-root").hidden,
          "message is hidden"
        );
      });
    }
  );
});

/**
 * Test to check onboarding message does not show when selecting "not now"
 *
 * Also confirms a Glean event was triggered.
 */
add_task(async function test_hideOnboarding_onClose() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  // OptedIn pref value is 0 when a user has not opted-in
  setOnboardingPrefs({ active: false, optedIn: 0, telemetryEnabled: true });
  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      // Get the actor to update the product URL, since no content will render without one
      let actor =
        gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getExistingActor(
          "ShoppingSidebar"
        );
      actor.updateProductURL("https://example.com/product/B09TJGHL5F");

      await SpecialPowers.spawn(browser, [], async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("shopping-container"),
          "shopping-container"
        );
        // "Not now" button
        let notNowButton = await ContentTaskUtils.waitForCondition(() =>
          shoppingContainer.querySelector(".additional-cta")
        );

        notNowButton.click();

        // Does not render shopping container onboarding message
        ok(
          !shoppingContainer.length,
          "Shopping container element does not exist"
        );
      });
    }
  );

  await Services.fog.testFlushAllChildren();
  let events = Glean.shopping.surfaceNotNowClicked.testGetValue();

  await BrowserTestUtils.waitForCondition(() => {
    let _events = Glean.shopping.surfaceNotNowClicked.testGetValue();
    return _events?.length > 0;
  });

  Assert.greater(events.length, 0);
  Assert.equal(events[0].category, "shopping");
  Assert.equal(events[0].name, "surface_not_now_clicked");
});

/**
 * Test to check behavior when selecting 'Yes, try it  to opt in to the
 * shopping experience.
 *
 * Also tests if a Glean event was correctly recorded.
 */
add_task(async function test_onOptIn() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  setOnboardingPrefs({ active: false, optedIn: 0, telemetryEnabled: true });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForMutationCondition(
          content.document,
          { childList: true, subtree: true },
          () => !!content.document.querySelector("shopping-container .primary")
        );

        // "Yes, try it" button
        let primary = content.document.querySelector(
          "shopping-container .primary"
        );
        primary.click();
      });
    }
  );

  await Services.fog.testFlushAllChildren();
  let events = Glean.shopping.surfaceOptInClicked.testGetValue();

  await BrowserTestUtils.waitForCondition(() => {
    let _events = Glean.shopping.surfaceOptInClicked.testGetValue();
    return _events?.length > 0;
  });

  Assert.greater(events.length, 0);
  Assert.equal(events[0].category, "shopping");
  Assert.equal(events[0].name, "surface_opt_in_clicked");
});

/**
 * Helper function to click the links in the Link Paragraph.
 */
async function linkParagraphClickLinks() {
  const sandbox = sinon.createSandbox();

  let handleActionStub = sandbox
    .stub(SpecialMessageActions, "handleAction")
    .withArgs(sandbox.match({ type: "OPEN_URL" }));

  let handleActionStubCalled = new Promise(resolve =>
    handleActionStub.callsFake(resolve)
  );

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForMutationCondition(
          content.document,
          { childList: true, subtree: true },
          // Can safely assume that if one of the link exists, they both do.
          () =>
            !!content.document.querySelector(
              ".legal-paragraph a[value='terms_of_use']"
            )
        );

        let termsOfUse = content.document.querySelector(
          "shopping-container .legal-paragraph a[value='terms_of_use']"
        );
        termsOfUse.click();
      });
    }
  );

  await handleActionStubCalled;

  handleActionStub.resetHistory();

  handleActionStubCalled = new Promise(resolve =>
    handleActionStub.callsFake(resolve)
  );

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForMutationCondition(
          content.document,
          { childList: true, subtree: true },
          // Can safely assume that if one of the link exists, they both do.
          () =>
            !!content.document.querySelector(
              ".legal-paragraph a[value='terms_of_use']"
            )
        );
        let privacyPolicy = content.document.querySelector(
          "shopping-container .legal-paragraph a[value='privacy_policy']"
        );
        privacyPolicy.click();
      });
    }
  );
  await handleActionStubCalled;

  handleActionStub.resetHistory();

  handleActionStubCalled = new Promise(resolve =>
    handleActionStub.callsFake(resolve)
  );

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForMutationCondition(
          content.document,
          { childList: true, subtree: true },
          () => content.document.querySelector(".link-paragraph a")
        );
        let learnMore = content.document.querySelector(
          "shopping-container .link-paragraph a[value='learn_more']"
        );
        // Learn More link button.
        learnMore.click();
      });
    }
  );
  await handleActionStubCalled;

  sandbox.restore();
}

/**
 * Test to check behavior when selecting links in the link-paragraph
 * to opt in to the
 * shopping experience.
 *
 * Also tests if a Glean event was correctly recorded.
 */
add_task(async function test_linkParagraph() {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
  setOnboardingPrefs({ active: false, optedIn: 0, telemetryEnabled: true });

  await linkParagraphClickLinks();

  await Services.fog.testFlushAllChildren();
  let privacyEvents =
    Glean.shopping.surfaceShowPrivacyPolicyClicked.testGetValue();

  Assert.greater(privacyEvents.length, 0);
  Assert.equal(privacyEvents[0].category, "shopping");
  Assert.equal(privacyEvents[0].name, "surface_show_privacy_policy_clicked");

  let tosEvents = Glean.shopping.surfaceShowTermsClicked.testGetValue();

  Assert.greater(tosEvents.length, 0);
  Assert.equal(tosEvents[0].category, "shopping");
  Assert.equal(tosEvents[0].name, "surface_show_terms_clicked");

  let learnMoreEvents = Glean.shopping.surfaceLearnMoreClicked.testGetValue();

  Assert.greater(learnMoreEvents.length, 0);
  Assert.equal(learnMoreEvents[0].category, "shopping");
  Assert.equal(learnMoreEvents[0].name, "surface_learn_more_clicked");
});

add_task(async function test_onboarding_auto_activate_opt_in() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        true,
      ],
    ],
  });
  // Opt out of the feature
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: 0,
    autoActivateCount: 0,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  // User is not opted-in, and auto-activate has not happened yet. So it should
  // be enabled now.
  ok(
    Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Global toggle should be activated to open the sidebar on PDPs"
  );

  // Now opt in, deactivate the global toggle, and reset the targeting prefs.
  // The sidebar should no longer open on PDPs, since the user is opted in and
  // the global toggle is off.

  setOnboardingPrefs({
    active: false,
    optedIn: 1,
    lastAutoActivate: 0,
    autoActivateCount: 1,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    !Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Global toggle should not activate again since user is opted in"
  );
});

add_task(async function test_onboarding_auto_activate_not_now() {
  // Opt of the feature so it auto-activates once.
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: 0,
    autoActivateCount: 0,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Global toggle should be activated to open the sidebar on PDPs"
  );

  // After auto-activating once, we should not auto-activate again in this
  // session. So when we click "Not now", it should deactivate the global
  // toggle, closing all sidebars, and sidebars should not open again on PDPs.
  // Test that handledAutoActivate was set automatically by the previous
  // auto-activate, and that it prevents the toggle from activating again.
  setOnboardingPrefs({ active: false });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    !Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Global toggle should not activate again this session"
  );

  // There are 3 conditions for auto-activating the sidebar before opt-in:
  // 1. The sidebar has not already been automatically set to `active` twice.
  // 2. It's been at least 24 hours since the user last saw the sidebar because
  //    of this auto-activation behavior.
  // 3. This method has not already been called (handledAutoActivate is false)
  // Let's test each of these conditions, in isolation.

  // Reset the auto-activate count to 0, and set the last auto-activate to never
  // opened. Leave the handledAutoActivate flag set to true, so we can
  // test that the sidebar auto-activate is still blocked if we already
  // auto-activated previously this session.
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: 0,
    autoActivateCount: 0,
    handledAutoActivate: true,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    !Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Shopping sidebar should not auto-activate if auto-activated previously this session"
  );

  // Now test that sidebar auto-activate is blocked if the last auto-activate
  // was less than 24 hours ago.
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: Date.now() / 1000,
    autoActivateCount: 1,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    !Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Shopping sidebar should not auto-activate if last auto-activation was less than 24 hours ago"
  );

  // Test that auto-activate is blocked if the sidebar has been auto-activated
  // twice already.
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: 0,
    autoActivateCount: 2,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    !Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Shopping sidebar should not auto-activate if it has already been auto-activated twice"
  );

  // Now test that auto-activate is unblocked if all 3 conditions are met.
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: Date.now() / 1000 - 2 * 24 * 60 * 60, // 2 days ago
    autoActivateCount: 1,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Shopping sidebar should auto-activate a second time if all conditions are met"
  );
});

/**
 * Test to check onboarding message is not shown for user
 * after a user opt-out and opt back in after seeing survey
 */

add_task(async function test_hideOnboarding_OptIn_AfterSurveySeen() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shopping.experience2023.optedIn", 0],
      ["browser.shopping.experience2023.survey.enabled", true],
      ["browser.shopping.experience2023.survey.hasSeen", true],
      ["browser.shopping.experience2023.survey.pdpVisits", 5],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: "about:shoppingsidebar",
      gBrowser,
    },
    async browser => {
      let actor =
        gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getExistingActor(
          "ShoppingSidebar"
        );
      actor.updateProductURL("https://example.com/product/B09TJGHL5F");

      await SpecialPowers.spawn(browser, [], async () => {
        let shoppingContainer = await ContentTaskUtils.waitForCondition(
          () => content.document.querySelector("shopping-container"),
          "shopping-container"
        );

        ok(
          !content.document.getElementById("multi-stage-message-root").hidden,
          "opt-in message is shown"
        );

        const { TestUtils } = ChromeUtils.importESModule(
          "resource://testing-common/TestUtils.sys.mjs"
        );

        let optedInPrefChanged = TestUtils.waitForPrefChange(
          "browser.shopping.experience2023.optedIn",
          value => value === 1
        );
        await SpecialPowers.pushPrefEnv({
          set: [["browser.shopping.experience2023.optedIn", 1]],
        });
        await optedInPrefChanged;
        await shoppingContainer.wrappedJSObject.updateComplete;

        ok(
          content.document.getElementById("multi-stage-message-root").hidden,
          "opt-in message is hidden"
        );
        await SpecialPowers.popPrefEnv();
      });
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_deactivate_sidebar_if_user_turns_off_cfr() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
    ],
  });
  // Opt out of the feature
  setOnboardingPrefs({
    active: false,
    optedIn: 0,
    lastAutoActivate: 0,
    autoActivateCount: 0,
    handledAutoActivate: false,
  });
  ShoppingUtils.handleAutoActivateOnProduct();

  ok(
    !Services.prefs.getBoolPref("browser.shopping.experience2023.active"),
    "Shopping sidebar should not auto-activate if Recommended features is turned off"
  );
});
