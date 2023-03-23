/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that are related to the accessibility of the feature callout
 */

/**
 * Ensure feature tour is accessible using a screen reader and with
 * keyboard navigation.
 */
add_task(async function feature_callout_is_accessible() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", getPrefValueByScreen(1)]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");

      await BrowserTestUtils.waitForCondition(
        () => document.activeElement.value === "primary_button",
        `Feature Callout primary button is focused on page load}`
      );
      ok(true, "Feature Callout primary button was focused on page load");

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
      await BrowserTestUtils.waitForCondition(
        () => document.activeElement.value == "primary_button",
        "Feature Callout primary button is focused after advancing screens"
      );
      ok(true, "Feature Callout primary button was successfully focused");
    }
  );
});
