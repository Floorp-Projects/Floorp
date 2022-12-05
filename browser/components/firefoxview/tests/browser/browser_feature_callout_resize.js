/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const featureTourPref = "browser.firefox-view.feature-tour";

add_setup(async function setup() {
  let originalWidth = window.outerWidth;
  let originalHeight = window.outerHeight;
  registerCleanupFunction(async () => {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:firefoxview" },
      async browser => window.FullZoom.reset(browser)
    );
    window.resizeTo(originalWidth, originalHeight);
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:firefoxview" },
    async browser => window.FullZoom.setZoom(0.5, browser)
  );
});

add_task(async function feature_callout_is_repositioned_if_it_does_not_fit() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.max_tabs_undo", 1]],
  });
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:firefoxview" },
    async browser => {
      const { document } = browser.contentWindow;

      browser.contentWindow.resizeTo(1550, 1000);
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On first screen at 1550x1000, the callout is positioned below the parent element"
      );

      let startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(1600, 400);
      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-start`),
        "On first screen at 1600x400, the callout is positioned to the right of the parent element"
      );

      startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(1100, 600);
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On first screen at 1100x600, the callout is positioned below the parent element"
      );
    }
  );
  sandbox.restore();
});

add_task(async function feature_callout_is_repositioned_rtl() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set layout direction to right to left
      ["intl.l10n.pseudo", "bidi"],
      ["browser.sessionstore.max_tabs_undo", 1],
    ],
  });

  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:firefoxview" },
    async browser => {
      const { document } = browser.contentWindow;

      browser.contentWindow.resizeTo(1550, 1000);
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On first screen at 1550x1000, the callout is positioned below the parent element"
      );

      let startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(1600, 400);
      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-end`),
        "On first screen at 1600x400, the callout is positioned to the right of the parent element"
      );

      startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(1100, 600);
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On first screen at 1100x600, the callout is positioned below the parent element"
      );
    }
  );
  sandbox.restore();
});
