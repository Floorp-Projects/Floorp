/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { calloutMessages } = ChromeUtils.importESModule(
  "chrome://browser/content/featureCallout.mjs"
);

const calloutSelector = "#root.featureCallout";

const waitForCalloutRender = async doc => {
  // Wait for callout to be rendered
  await BrowserTestUtils.waitForMutationCondition(
    doc.body,
    { childList: true },
    () => doc.body.querySelector(calloutSelector)
  );
};

const waitForCalloutPositioned = async doc => {
  // When the callout has a style set for "top" it has
  // been positioned, which means screen content has rendered
  const callout = doc.querySelector(calloutSelector);
  await BrowserTestUtils.waitForMutationCondition(
    callout,
    { attributeFilter: ["style"], attributes: true },
    () => {
      return callout.style.top;
    }
  );
};

const waitForCalloutScreen = async (doc, screenId) => {
  await BrowserTestUtils.waitForMutationCondition(
    doc.querySelector(calloutSelector),
    { attributeFilter: ["style"], attributes: true },
    () => doc.querySelector(screenId)
  );
};

const clickPrimaryButton = doc => {
  doc.querySelector(".action-buttons .primary").click();
};

add_task(async function feature_callout_renders_in_firefox_view() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", "{}"]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );
    }
  );
});

add_task(async function feature_callout_moves_on_screen_change() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", "{}"]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      const buttonSelector = "#root .primary";

      await waitForCalloutRender(document);

      const callout = document.querySelector(calloutSelector);

      // When the callout has a style set for "top" it has
      // been positioned, which means screen content has rendered
      let startingTop;
      await BrowserTestUtils.waitForMutationCondition(
        callout,
        { attributeFilter: ["style"], attributes: true },
        () => {
          startingTop = callout.style.top;
          return callout.style.top;
        }
      );

      document.querySelector(buttonSelector).click();

      // Wait for callout to be repositioned
      await BrowserTestUtils.waitForMutationCondition(
        callout,
        { attributeFilter: ["style"], attributes: true },
        () => callout.style.top != startingTop
      );

      ok(
        startingTop !== callout.style.top,
        "Feature Callout moves to a new element when a user clicks the primary button"
      );
    }
  );
});

add_task(async function feature_callout_is_not_show_twice() {
  // Third comma-separated value of the pref is set to a string value once a user completes the tour
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.firefox-view.feature-tour",
        '{"message":"","screen":"","complete":true}',
      ],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        !document.querySelector(calloutSelector),
        "Feature Callout tour does not render if the user finished it previously"
      );
    }
  );
});

add_task(
  async function feature_callout_first_screen_positioned_below_element() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.firefox-view.feature-tour", "{}"]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        await waitForCalloutRender(document);
        await waitForCalloutPositioned(document);
        let parentTop = document
          .querySelector("#tabpickup-steps")
          .getBoundingClientRect().top;
        let containerTop = document
          .querySelector(calloutSelector)
          .getBoundingClientRect().top;
        ok(
          parentTop < containerTop,
          `${parentTop} ${containerTop} Feature Callout is positioned below parent element when callout is configured to point from the top`
        );
      }
    );
  }
);

add_task(
  async function feature_callout_second_screen_positioned_above_element() {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.firefox-view.feature-tour", "{}"]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        await waitForCalloutRender(document);
        await waitForCalloutPositioned(document);
        // Advance to second screen
        clickPrimaryButton(document);
        await waitForCalloutScreen(document, ".FEATURE_CALLOUT_2");

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
      set: [["browser.firefox-view.feature-tour", "{}"]],
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        await waitForCalloutRender(document);
        await waitForCalloutPositioned(document);
        // Advance to third screen
        clickPrimaryButton(document);
        await waitForCalloutScreen(document, ".FEATURE_CALLOUT_2");
        clickPrimaryButton(document);
        await waitForCalloutScreen(document, ".FEATURE_CALLOUT_3");

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
        ["browser.firefox-view.feature-tour", "{}"],
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

        await waitForCalloutRender(document);
        await waitForCalloutPositioned(document);
        // Advance to third screen
        clickPrimaryButton(document);
        await waitForCalloutScreen(document, ".FEATURE_CALLOUT_2");
        clickPrimaryButton(document);
        await waitForCalloutScreen(document, ".FEATURE_CALLOUT_3");

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
  }
);
