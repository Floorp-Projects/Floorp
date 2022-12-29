/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const featureTourPref = "browser.firefox-view.feature-tour";
const defaultPrefValue = getPrefValueByScreen(1);

add_task(
  async function feature_callout_first_screen_positioned_below_element() {
    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
    );
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
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
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_second_screen_positioned_left_of_element() {
    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_FEATURE_TOUR_2_NO_CWS"
    );
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
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
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_third_screen_position_respects_RTL_layouts() {
    await SpecialPowers.pushPrefEnv({
      set: [
        // Set layout direction to right to left
        ["intl.l10n.pseudo", "bidi"],
      ],
    });

    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_FEATURE_TOUR_2_NO_CWS"
    );
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
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
    sandbox.restore();
  }
);

add_task(
  async function feature_callout_is_repositioned_if_parent_container_is_toggled() {
    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
    );
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
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
        await closeCallout(document);
      }
    );
    sandbox.restore();
  }
);

// This test should be moved into a surface agnostic test suite with bug 1793656.
add_task(async function feature_callout_top_end_positioning() {
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  testMessage.message.content.screens[0].content.arrow_position = "top-end";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      let parent = document.querySelector("#tab-pickup-container");
      let container = document.querySelector(calloutSelector);
      let parentLeft = parent.getBoundingClientRect().left;
      let containerLeft = container.getBoundingClientRect().left;

      ok(
        container.classList.contains("arrow-top-end"),
        "Feature Callout container has the expected arrow-top-end class"
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
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  testMessage.message.content.screens[0].content.arrow_position = "top-start";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      let parent = document.querySelector("#tab-pickup-container");
      let container = document.querySelector(calloutSelector);
      let parentLeft = parent.getBoundingClientRect().left;
      let containerLeft = container.getBoundingClientRect().left;

      ok(
        container.classList.contains("arrow-top-start"),
        "Feature Callout container has the expected arrow-top-start class"
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
      ],
    });

    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
    );
    testMessage.message.content.screens[0].content.arrow_position = "top-end";
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
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

        await closeCallout(document);
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
  async function feature_callout_smaller_parent_container_than_callout_container() {
    let testMessage = {
      message: {
        id: "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS",
        template: "feature_callout",
        content: {
          id: "FIREFOX_VIEW_FEATURE_TOUR",
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
          ],
        },
      },
    };

    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

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
        await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
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
  }
);
