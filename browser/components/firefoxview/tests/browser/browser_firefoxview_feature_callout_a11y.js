/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that are related to the accessibility of the feature callout
 */
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

/**
 * Ensure feature tour is accessible using a screen reader and with
 * keyboard navigation.
 */
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
      browser.contentWindow.resizeTo(1550, 1000);

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");

      await BrowserTestUtils.waitForCondition(
        () => document.activeElement.id === calloutId,
        "Feature Callout is focused on page load"
      );
      ok(true, "Feature Callout was focused on page load");
      await BrowserTestUtils.waitForCondition(
        () =>
          document.querySelector(
            `${calloutSelector}[aria-describedby="#${calloutId} .welcome-text"]`
          ),
        "The callout container has an aria-describedby value equal to the screen welcome text"
      );
      ok(true, "The callout container has the correct aria-describedby value");

      // Advance to second screen
      clickPrimaryButton(document);
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
      ok(true, "FEATURE_CALLOUT_2 was successfully displayed");

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
      ok(true, "Feature Callout was successfully focused");
    }
  );
});
