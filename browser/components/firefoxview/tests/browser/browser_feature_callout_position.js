/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { FeatureCalloutMessages } = ChromeUtils.import(
  "resource://activity-stream/lib/FeatureCalloutMessages.jsm"
);

const calloutId = "root";
const calloutSelector = `#${calloutId}.featureCallout`;
const primaryButtonSelector = `#${calloutId} .primary`;
const featureTourPref = "browser.firefox-view.feature-tour";
const getPrefValueByScreen = screen => {
  return JSON.stringify({
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

// This test should be moved into a surface agnostic test suite with bug 1793656.
add_task(async function feature_callout_top_end_positioning() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, getPrefValueByScreen(1)]],
  });

  const testMessage = {
    message: FeatureCalloutMessages.getMessages().find(
      m => m.id === "FIREFOX_VIEW_FEATURE_TOUR_1"
    ),
  };
  testMessage.message.content.screens[0].content.arrow_position = "top-end";

  const sandbox = sinon.createSandbox();
  const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
  sendTriggerStub.resolves(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, 1);
      let parent = document.querySelector("#tab-pickup-container");
      let container = document.querySelector(calloutSelector);
      let parentLeft = parent.getBoundingClientRect().left;
      let containerLeft = container.getBoundingClientRect().left;

      ok(
        container.classList.contains("arrow-top-end"),
        "Feature Callout container has the expected arrow-top-end class"
      );
      ok(
        containerLeft - parent.clientWidth + container.offsetWidth ===
          parentLeft,
        "Feature Callout's right edge is aligned with parent element's right edge"
      );
    }
  );
  sandbox.restore();
});

// This test should be moved into a surface agnostic test suite with bug 1793656.
add_task(
  async function feature_callout_top_end_position_respects_RTL_layouts() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [featureTourPref, getPrefValueByScreen(1)],
        // Set layout direction to right to left
        ["intl.l10n.pseudo", "bidi"],
      ],
    });

    const testMessage = {
      message: FeatureCalloutMessages.getMessages().find(
        m => m.id === "FIREFOX_VIEW_FEATURE_TOUR_1"
      ),
    };
    testMessage.message.content.screens[0].content.arrow_position = "top-end";

    const sandbox = sinon.createSandbox();
    const sendTriggerStub = sandbox.stub(ASRouter, "sendTriggerMessage");
    sendTriggerStub.resolves(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, 1);
        let parent = document.querySelector("#tab-pickup-container");
        let container = document.querySelector(calloutSelector);
        let parentLeft = parent.getBoundingClientRect().left;
        let containerLeft = container.getBoundingClientRect().left;

        ok(
          container.classList.contains("arrow-top-start"),
          "In RTL mode, the feature Callout container has the expected arrow-top-start class"
        );
        ok(
          containerLeft === parentLeft,
          "In RTL mode, the feature Callout's left edge is aligned with parent element's left edge"
        );
      }
    );

    await SpecialPowers.pushPrefEnv({
      set: [
        // Revert layout direction to left to right
        ["intl.l10n.pseudo", ""],
      ],
    });

    sandbox.restore();
  }
);
