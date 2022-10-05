/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const calloutId = "root";
const calloutSelector = `#${calloutId}.featureCallout`;
const primaryButtonSelector = `#${calloutId} .primary`;
const featureTourPref = "browser.firefox-view.feature-tour";
const getPrefValueByScreen = screen => {
  return JSON.stringify({
    message: "FIREFOX_VIEW_FEATURE_TOUR",
    screen: `FEATURE_CALLOUT_${screen}`,
    complete: false,
  });
};

const waitForCalloutScreen = async (doc, screenNumber) => {
  await BrowserTestUtils.waitForCondition(() => {
    return doc.querySelector(
      `${calloutSelector}:not(.hidden) .FEATURE_CALLOUT_${screenNumber}`
    );
  });
};

const clickPrimaryButton = async doc => {
  doc.querySelector(primaryButtonSelector).click();
};

add_setup(async function setup() {
  let originalWidth = window.outerWidth;
  let originalHeight = window.outerHeight;
  registerCleanupFunction(async () => {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => window.FullZoom.reset(browser)
    );
    window.resizeTo(originalWidth, originalHeight);
  });
});

// Tour is accessible using a screen reader and keyboard navigation
add_task(async function feature_callout_is_accessible() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.firefox-view.feature-tour", getPrefValueByScreen(1)],
      ["browser.sessionstore.max_tabs_undo", 1],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      window.FullZoom.setZoom(0.5, browser);
      browser.contentWindow.resizeTo(1500, 1000);

      await waitForCalloutScreen(document, 1);

      await BrowserTestUtils.waitForCondition(
        () => document.activeElement.id === calloutId,
        "Feature Callout is focused on page load"
      );
      await BrowserTestUtils.waitForCondition(
        () => document.querySelector(`${calloutSelector}[role="alert"]`),
        "The callout container has role of alert"
      );
      await BrowserTestUtils.waitForCondition(
        () =>
          document.querySelector(
            `${calloutSelector}[aria-describedby="#${calloutId} .welcome-text"]`
          ),
        "The callout container has an aria-describedby value equal to the screen welcome text"
      );

      // Advance to second screen
      clickPrimaryButton(document);
      await waitForCalloutScreen(document, 2);

      const startingTop = document.querySelector(calloutSelector).style.top;

      browser.contentWindow.resizeTo(800, 800);
      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );

      await BrowserTestUtils.waitForCondition(
        () => document.activeElement.id === calloutId,
        "Feature Callout is focused after advancing screens"
      );
      await BrowserTestUtils.waitForCondition(
        () => document.querySelector(`${calloutSelector}[role="alert"]`),
        "The callout container has role of alert after advancing screens"
      );
    }
  );
});

add_task(async function feature_callout_is_repositioned_if_it_does_not_fit() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.firefox-view.feature-tour", getPrefValueByScreen(1)],
      ["browser.sessionstore.max_tabs_undo", 1],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      browser.contentWindow.resizeTo(1500, 1000);
      await waitForCalloutScreen(document, 1);
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On first screen at 1500x1000, the callout is positioned below the parent element"
      );

      let startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(1500, 600);
      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-start`),
        "On first screen at 1500x600, the callout is positioned to the right of the parent element"
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

      clickPrimaryButton(document);
      await waitForCalloutScreen(document, 2);
      clickPrimaryButton(document);
      await waitForCalloutScreen(document, 3);

      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-end`),
        "On third screen at 1100x600, the callout is positioned at the start of the parent element originally configured"
      );

      startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(800, 800);
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-bottom`),
        "On third screen at 800x800, the callout is positioned above the parent element"
      );

      startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(700, 1150);
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On third screen at 700x1150, the callout is positioned below the parent element"
      );
    }
  );
});

add_task(async function feature_callout_is_repositioned_rtl() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [featureTourPref, getPrefValueByScreen(1)],
      // Set layout direction to right to left
      ["intl.l10n.pseudo", "bidi"],
      ["browser.sessionstore.max_tabs_undo", 1],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      browser.contentWindow.resizeTo(1500, 1000);
      await waitForCalloutScreen(document, 1);
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On first screen at 1500x1000, the callout is positioned below the parent element"
      );

      let startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(1500, 600);
      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-end`),
        "On first screen at 1500x600, the callout is positioned to the right of the parent element"
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

      clickPrimaryButton(document);
      await waitForCalloutScreen(document, 2);
      clickPrimaryButton(document);
      await waitForCalloutScreen(document, 3);

      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-start`),
        "On third screen at 1100x600, the callout is positioned at the start of the parent element originally configured"
      );

      startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(800, 800);
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-bottom`),
        "On third screen at 800x800, the callout is positioned above the parent element"
      );

      startingTop = document.querySelector(calloutSelector).style.top;
      browser.contentWindow.resizeTo(700, 1150);
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );
      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On third screen at 700x1150, the callout is positioned below the parent element"
      );
    }
  );
});
