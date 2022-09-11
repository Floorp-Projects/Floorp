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
const defaultPrefValue = getPrefValueByScreen(1);

const waitForCalloutScreen = async (doc, screenNumber) => {
  await BrowserTestUtils.waitForCondition(() => {
    return doc.querySelector(
      `${calloutSelector}:not(.hidden) .FEATURE_CALLOUT_${screenNumber}`
    );
  });
};

add_task(
  async function feature_callout_first_screen_positioned_below_element() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, defaultPrefValue]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, 1);
        let parentTop = document
          .querySelector("#tab-pickup-container")
          .getBoundingClientRect().top;
        let containerTop = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().top;
        ok(
          parentTop < containerTop,
          "Feature Callout is positioned below parent element when callout is configured to point from the top"
        );
      }
    );
  }
);

add_task(
  async function feature_callout_second_screen_positioned_above_element() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, getPrefValueByScreen(2)]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, 2);
        let parentTop = document
          .querySelector("#recently-closed-tabs-container")
          .getBoundingClientRect().top;
        let containerTop = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().top;

        ok(
          parentTop > containerTop,
          "Feature Callout is positioned above parent element when callout is configured to point from the bottom"
        );
      }
    );
  }
);

add_task(
  async function feature_callout_third_screen_positioned_left_of_element() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, getPrefValueByScreen(3)]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, 3);
        let parentLeft = document
          .querySelector("#colorways.content-container")
          .getBoundingClientRect().left;
        let containerLeft = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().left;

        ok(
          parentLeft > containerLeft,
          "Feature Callout is positioned left of parent element when callout is configured at end of callout"
        );
      }
    );
  }
);

add_task(
  async function feature_callout_third_screen_position_respects_RTL_layouts() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [featureTourPref, getPrefValueByScreen(3)],
        // Set layout direction to right to left
        ["intl.l10n.pseudo", "bidi"],
      ],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, 3);
        let parentLeft = document
          .querySelector("#colorways")
          .getBoundingClientRect().left;
        let containerLeft = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().left;

        ok(
          parentLeft < containerLeft,
          "Feature Callout is positioned right of parent element when callout is configured at end of callout in right to left layouts"
        );
      }
    );

    await SpecialPowers.pushPrefEnv({
      set: [
        // Revert layout direction to left to right
        ["intl.l10n.pseudo", ""],
      ],
    });
  }
);
