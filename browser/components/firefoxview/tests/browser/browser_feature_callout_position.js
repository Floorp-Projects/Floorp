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
          .querySelector("#colorways")
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

add_task(
  async function feature_callout_is_repositioned_if_parent_container_is_toggled() {
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
        const parentEl = document.querySelector("#tab-pickup-container");
        const calloutStartingTopPosition = document.querySelector(
          calloutSelector
        ).style.top;

        //container has been toggled/minimized
        parentEl.removeAttribute("open", "");
        await BrowserTestUtils.waitForMutationCondition(
          document.querySelector(calloutSelector),
          { attributes: true },
          () =>
            document.querySelector(calloutSelector).style.top !=
            calloutStartingTopPosition
        );
        ok(
          document.querySelector(calloutSelector).style.top !=
            calloutStartingTopPosition,
          "Feature Callout position is recalculated when parent element is toggled"
        );
      }
    );
  }
);

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

add_task(
  async function feature_callout_custom_position_override_properties_are_applied() {
    const testMessage = {
      message: FeatureCalloutMessages.getMessages().find(
        m => m.id === "FIREFOX_VIEW_FEATURE_TOUR_1"
      ),
    };
    testMessage.message.content.screens[0].content.callout_position_override = {
      top: "500px",
      left: "500px",
    };

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
        let container = document.querySelector(calloutSelector);
        let containerLeft = container.getBoundingClientRect().left;
        let containerTop = container.getBoundingClientRect().top;
        ok(
          containerLeft === 500 && containerTop === 500,
          "Feature callout container has a top position of 500, and left position of 500"
        );
      }
    );

    sandbox.restore();
  }
);

add_task(
  async function feature_callout_smaller_parent_container_than_callout_container() {
    await ASRouter.waitForInitialized;

    let message = {
      weight: 100,
      id: "FIREFOX_VIEW_FEATURE_TOUR_1",
      template: "feature_callout",
      content: {
        id: "FIREFOX_VIEW_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FEATURE_CALLOUT_1",
            parent_selector: "#colorways-button",
            content: {
              position: "callout",
              arrow_position: "end",
              title: "callout-firefox-view-colorways-reminder-title",
              subtitle: {
                string_id: "callout-firefox-view-colorways-reminder-subtitle",
              },
              logo: {
                imageURL: "chrome://browser/content/callout-tab-pickup.svg",
                darkModeImageURL:
                  "chrome://browser/content/callout-tab-pickup-dark.svg",
                height: "128px", //#colorways-button has a height of 35px
              },
              dismiss_button: {
                action: {
                  navigate: true,
                },
              },
            },
          },
          { content: {} },
          { content: {} },
        ],
      },
      priority: 1,
      targeting: 'source == "firefoxview"',
      trigger: {
        id: "featureCalloutCheck",
      },
      groups: [],
      provider: "onboarding",
    };
    let previousMessages = ASRouter.state.messages;
    ASRouter.setState({ messages: [message] });

    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, getPrefValueByScreen(1)]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, 1);
        let parentHeight = document.querySelector("#colorways-button")
          .offsetHeight;
        let containerHeight = document.querySelector(calloutSelector)
          .offsetHeight;

        let parentPositionTop =
          document.querySelector("#colorways-button").getBoundingClientRect()
            .top + window.scrollY;
        let containerPositionTop =
          document.querySelector(calloutSelector).getBoundingClientRect().top +
          window.scrollY;
        ok(
          containerHeight > parentHeight,
          "Feature Callout is height is larger than parent element when callout is configured at end of callout"
        );
        ok(
          containerPositionTop < parentPositionTop,
          "Feature Callout is positioned higher that parent element when callout is configured at end of callout"
        );
        ok(
          // difference in centering may be off due to rounding, thus +/- 1px for
          // eslint-disable-next-line prettier/prettier
          Math.abs(((containerHeight / 2) + containerPositionTop) - ((parentHeight / 2) + parentPositionTop)) <= 1,
          "Feature Callout is centered equally to parent element when callout is configured at end of callout"
        );
        await ASRouter.setState({ messages: [...previousMessages] });
        await ASRouter.resetMessageState();
      }
    );
  }
);
