/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const defaultPrefValue = getPrefValueByScreen(1);

const squareWidth = 24;
const arrowWidth = Math.hypot(squareWidth, squareWidth);
const arrowHeight = arrowWidth / 2;
let overlap = 5 - arrowHeight;

add_task(
  async function feature_callout_first_screen_positioned_below_element() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, defaultPrefValue]],
    });
    const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
        let parentBottom = document
          .querySelector("#tab-pickup-container")
          .getBoundingClientRect().bottom;
        let containerTop = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().top;

        isfuzzy(
          parentBottom - containerTop,
          overlap,
          1, // add 1px fuzziness to account for possible subpixel rounding
          "Feature Callout is positioned below parent element with the arrow overlapping by 5px"
        );
      }
    );
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_second_screen_positioned_right_of_element() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, getPrefValueByScreen(2)]],
    });
    const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
    testMessage.message.content.screens[1].anchors = [
      { selector: ".brand-logo", arrow_position: "start" },
    ];
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");

        let parentRight = document
          .querySelector(".brand-logo")
          .getBoundingClientRect().right;
        let containerLeft = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().left;
        isfuzzy(
          parentRight - containerLeft,
          overlap,
          1,
          "Feature Callout is positioned right of parent element with the arrow overlapping by 5px"
        );
      }
    );
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_second_screen_positioned_above_element() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, getPrefValueByScreen(2)]],
    });
    const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
        let parentTop = document
          .querySelector("#recently-closed-tabs-container")
          .getBoundingClientRect().top;
        let containerBottom = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().bottom;

        Assert.greaterOrEqual(
          parentTop,
          containerBottom - 5 - 1,
          "Feature Callout is positioned above parent element with 5px overlap"
        );
      }
    );
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_third_screen_position_respects_RTL_layouts() {
    await SpecialPowers.pushPrefEnv({
      set: [
        // Set layout direction to right to left
        ["intl.l10n.pseudo", "bidi"],
        [featureTourPref, getPrefValueByScreen(2)],
      ],
    });

    const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        const parent = document.querySelector(
          "#recently-closed-tabs-container"
        );
        parent.style.gridArea = "1/2";
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
        let parentRight = parent.getBoundingClientRect().right;
        let containerLeft = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().left;

        Assert.lessOrEqual(
          parentRight,
          containerLeft + 5 + 1,
          "Feature Callout is positioned right of parent element when callout is set to 'end' in RTL layouts"
        );
      }
    );

    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_is_repositioned_if_parent_container_is_toggled() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, defaultPrefValue]],
    });
    const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
        const parentEl = document.querySelector("#tab-pickup-container");
        const calloutStartingTopPosition =
          document.querySelector(calloutSelector).style.top;

        //container has been toggled/minimized
        parentEl.removeAttribute("open", "");
        await BrowserTestUtils.waitForMutationCondition(
          document.querySelector(calloutSelector),
          { attributes: true },
          () =>
            document.querySelector(calloutSelector).style.top !=
            calloutStartingTopPosition
        );
        isnot(
          document.querySelector(calloutSelector).style.top,
          calloutStartingTopPosition,
          "Feature Callout position is recalculated when parent element is toggled"
        );
        await closeCallout(document);
      }
    );
    sandbox.restore();
  }
);

// This test should be moved into a surface agnostic test suite with bug 1793656.
add_task(async function feature_callout_top_end_positioning() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, defaultPrefValue]],
  });
  const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  testMessage.message.content.screens[0].anchors[0].arrow_position = "top-end";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      let parent = document.querySelector("#tab-pickup-container");
      let container = document.querySelector(calloutSelector);
      let parentLeft = parent.getBoundingClientRect().left;
      let containerLeft = container.getBoundingClientRect().left;

      is(
        container.getAttribute("arrow-position"),
        "top-end",
        "Feature Callout container has the expected top-end arrow-position attribute"
      );
      isfuzzy(
        containerLeft - parent.clientWidth + container.offsetWidth,
        parentLeft,
        1, // Display scaling can cause up to 1px difference in layout
        "Feature Callout's right edge is approximately aligned with parent element's right edge"
      );

      await closeCallout(document);
    }
  );
  sandbox.restore();
});

// This test should be moved into a surface agnostic test suite with bug 1793656.
add_task(async function feature_callout_top_start_positioning() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, defaultPrefValue]],
  });
  const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  testMessage.message.content.screens[0].anchors[0].arrow_position =
    "top-start";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      let parent = document.querySelector("#tab-pickup-container");
      let container = document.querySelector(calloutSelector);
      let parentLeft = parent.getBoundingClientRect().left;
      let containerLeft = container.getBoundingClientRect().left;

      is(
        container.getAttribute("arrow-position"),
        "top-start",
        "Feature Callout container has the expected top-start arrow-position attribute"
      );
      isfuzzy(
        containerLeft,
        parentLeft,
        1, // Display scaling can cause up to 1px difference in layout
        "Feature Callout's left edge is approximately aligned with parent element's left edge"
      );

      await closeCallout(document);
    }
  );
  sandbox.restore();
});

// This test should be moved into a surface agnostic test suite with bug 1793656.
add_task(
  async function feature_callout_top_end_position_respects_RTL_layouts() {
    await SpecialPowers.pushPrefEnv({
      set: [
        // Set layout direction to right to left
        ["intl.l10n.pseudo", "bidi"],
        [featureTourPref, defaultPrefValue],
      ],
    });

    const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
    testMessage.message.content.screens[0].anchors[0].arrow_position =
      "top-end";
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
        let parent = document.querySelector("#tab-pickup-container");
        let container = document.querySelector(calloutSelector);
        let parentLeft = parent.getBoundingClientRect().left;
        let containerLeft = container.getBoundingClientRect().left;

        is(
          container.getAttribute("arrow-position"),
          "top-start",
          "In RTL mode, the feature callout container has the expected top-start arrow-position attribute"
        );
        is(
          containerLeft,
          parentLeft,
          "In RTL mode, the feature Callout's left edge is aligned with parent element's left edge"
        );

        await closeCallout(document);
      }
    );

    await SpecialPowers.popPrefEnv();
    sandbox.restore();
  }
);

add_task(async function feature_callout_is_larger_than_its_parent() {
  let testMessage = {
    message: {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      template: "feature_callout",
      content: {
        id: "FIREFOX_VIEW_FEATURE_TOUR",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FEATURE_CALLOUT_1",
            anchors: [
              { selector: ".brand-feature-name", arrow_position: "end" },
            ],
            content: {
              position: "callout",
              title: "callout-firefox-view-tab-pickup-title",
              subtitle: {
                string_id: "callout-firefox-view-tab-pickup-subtitle",
              },
              logo: {
                imageURL: "chrome://browser/content/callout-tab-pickup.svg",
                darkModeImageURL:
                  "chrome://browser/content/callout-tab-pickup-dark.svg",
                height: "128px", // .brand-feature-name has a height of 32px
              },
              dismiss_button: {
                action: {
                  navigate: true,
                },
              },
            },
          },
        ],
      },
    },
  };

  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

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

      launchFeatureTourIn(browser.contentWindow);

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      let parent = document.querySelector(".brand-feature-name");
      let container = document.querySelector(calloutSelector);
      let parentHeight = parent.offsetHeight;
      let containerHeight = container.offsetHeight;

      let parentPositionTop =
        parent.getBoundingClientRect().top + window.scrollY;
      let containerPositionTop =
        container.getBoundingClientRect().top + window.scrollY;
      Assert.greater(
        containerHeight,
        parentHeight,
        "Feature Callout is height is larger than parent element when callout is configured at end of callout"
      );
      Assert.less(
        containerPositionTop,
        parentPositionTop,
        "Feature Callout is positioned higher that parent element when callout is configured at end of callout"
      );
      isfuzzy(
        containerHeight / 2 + containerPositionTop,
        parentHeight / 2 + parentPositionTop,
        1, // Display scaling can cause up to 1px difference in layout
        "Feature Callout is centered equally to parent element when callout is configured at end of callout"
      );
      await ASRouter.resetMessageState();
    }
  );
  sandbox.restore();
});
