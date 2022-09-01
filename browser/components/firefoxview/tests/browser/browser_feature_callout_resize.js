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

// Tour is accessible using a screen reader and keyboard navigation
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
      let startHeight = window.outerHeight;
      let startWidth = window.outerWidth;

      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, 1);

      ok(
        (document.activeElement.id = calloutId),
        "Feature Callout is focused on page load"
      );
      ok(
        document.querySelector(`${calloutSelector}[role="alert"]`),
        "The callout container has role of alert"
      );
      ok(
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

      ok(
        (document.activeElement.id = calloutId),
        "Feature Callout is focused after advancing screens"
      );
      ok(
        document.querySelector(`${calloutSelector}[role="alert"]`),
        "The callout container has role of alert after advancing screens"
      );

      browser.contentWindow.resizeTo(startWidth, startHeight);
    }
  );
});

add_task(async function feature_callout_is_repositioned_if_it_does_not_fit() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", getPrefValueByScreen(3)]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      let startHeight = window.outerHeight;
      let startWidth = window.outerWidth;

      const { document } = browser.contentWindow;

      await waitForCalloutScreen(document, 3);

      let startingTop = document.querySelector(calloutSelector).style.top;

      browser.contentWindow.resizeTo(1200, 800);

      ok(
        document.querySelector(`${calloutSelector}.arrow-inline-end`),
        "On third screen, the callout is positioned at the start of the parent element originally configured"
      );

      startingTop = document.querySelector(calloutSelector).style.top;

      browser.contentWindow.resizeTo(800, 800);

      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        document.querySelector(calloutSelector),
        { attributeFilter: ["style"], attributes: true },
        () => document.querySelector(calloutSelector).style.top != startingTop
      );

      ok(
        document.querySelector(`${calloutSelector}.arrow-top`),
        "On third screen at a narrower window width, the callout is positioned below the parent element"
      );

      browser.contentWindow.resizeTo(startWidth, startHeight);
    }
  );
});
