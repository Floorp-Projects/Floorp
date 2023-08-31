"use strict";

add_task(
  async function test_firefox_view_tab_pick_up_not_signed_in_targeting() {
    ASRouter.resetMessageState();

    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.firefox-view.feature-tour", `{"screen":"","complete":true}`],
      ],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["browser.firefox-view.view-count", 3]],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["identity.fxaccounts.enabled", false]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(
          document,
          "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
        );
        ok(
          document.querySelector(".featureCallout"),
          "Firefox:View Tab Pickup should be displayed."
        );

        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
      }
    );
  }
);

add_task(
  async function test_firefox_view_tab_pick_up_sync_not_enabled_targeting() {
    ASRouter.resetMessageState();

    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.firefox-view.feature-tour", `{"screen":"","complete":true}`],
      ],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["browser.firefox-view.view-count", 3]],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["identity.fxaccounts.enabled", true]],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["services.sync.engine.tabs", false]],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["services.sync.username", false]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(
          document,
          "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
        );
        ok(
          document.querySelector(".featureCallout"),
          "Firefox:View Tab Pickup should be displayed."
        );

        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
      }
    );
  }
);

add_task(
  async function test_firefox_view_tab_pick_up_wait_24_hours_after_spotlight() {
    const TWENTY_FIVE_HOURS_IN_MS = 25 * 60 * 60 * 1000;

    ASRouter.resetMessageState();

    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.firefox-view.feature-tour", `{"screen":"","complete":true}`],
      ],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["browser.firefox-view.view-count", 3]],
    });

    await SpecialPowers.pushPrefEnv({
      set: [["identity.fxaccounts.enabled", false]],
    });

    ASRouter.setState({
      messageImpressions: { FIREFOX_VIEW_SPOTLIGHT: [Date.now()] },
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        ok(
          !document.querySelector(".featureCallout"),
          "Tab Pickup reminder should not be displayed when the Spotlight message introducing the tour was viewed less than 24 hours ago."
        );
      }
    );

    ASRouter.setState({
      messageImpressions: {
        FIREFOX_VIEW_SPOTLIGHT: [Date.now() - TWENTY_FIVE_HOURS_IN_MS],
      },
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(
          document,
          "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
        );
        ok(
          document.querySelector(".featureCallout"),
          "Tab Pickup reminder can be displayed when the Spotlight message introducing the tour was viewed over 24 hours ago."
        );

        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
      }
    );
  }
);
