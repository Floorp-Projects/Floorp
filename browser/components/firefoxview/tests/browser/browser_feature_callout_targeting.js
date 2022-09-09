"use strict";

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const calloutId = "root";
const calloutSelector = `#${calloutId}.featureCallout`;
const waitForCalloutScreen = async (doc, screenPostfix) => {
  await BrowserTestUtils.waitForCondition(() => {
    return doc.querySelector(
      `${calloutSelector}:not(.hidden) .FIREFOX_VIEW_${screenPostfix}`
    );
  });
};

add_task(async function test_firefox_view_colorways_reminder_targeting() {
  const sandbox = sinon.createSandbox();

  ASRouter.resetMessageState();

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.firefox-view.feature-tour",
        `{"message":"","screen":"","complete":true}`,
      ],
    ],
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.view-count", 4]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      await waitForCalloutScreen(document, "COLORWAYS_REMINDER");
      ok(
        document.querySelector(".featureCallout"),
        "FirefoxView Colorways Reminder should be displayed."
      );

      sandbox.restore();
      SpecialPowers.popPrefEnv();
      SpecialPowers.popPrefEnv();
    }
  );
});

add_task(
  async function test_firefox_view_tab_pick_up_not_signed_in_targeting() {
    const sandbox = sinon.createSandbox();
    ASRouter.resetMessageState();

    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.firefox-view.feature-tour",
          `{"message":"","screen":"","complete":true}`,
        ],
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

        await waitForCalloutScreen(document, "TAB_PICKUP_REMINDER");
        ok(
          document.querySelector(".featureCallout"),
          "Firefox:View Tab Pickup should be displayed."
        );

        sandbox.restore();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
      }
    );
  }
);

add_task(
  async function test_firefox_view_tab_pick_up_sync_not_enabled_targeting() {
    const sandbox = sinon.createSandbox();
    ASRouter.resetMessageState();

    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.firefox-view.feature-tour",
          `{"message":"","screen":"","complete":true}`,
        ],
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

        await waitForCalloutScreen(document, "TAB_PICKUP_REMINDER");
        ok(
          document.querySelector(".featureCallout"),
          "Firefox:View Tab Pickup should be displayed."
        );

        sandbox.restore();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
        SpecialPowers.popPrefEnv();
      }
    );
  }
);
