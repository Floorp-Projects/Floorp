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
  await BrowserTestUtils.waitForMutationCondition(
    doc.querySelector(calloutSelector),
    { attributeFilter: ["style"], attributes: true },
    () => {
      return doc.querySelector(calloutSelector).style.top;
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

      // Wait for callout to be rendered
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

      waitForCalloutScreen(document, "FEATURE_CALLOUT_2");

      ok(
        startingTop !== document.querySelector(calloutSelector).style.top,
        "Feature Callout moves to a new element when a user clicks the primary button"
      );
    }
  );
});

add_task(async function feature_callout_is_not_shown_twice() {
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

add_task(async function feature_callout_syncs_across_visits_and_tabs() {
  // Second comma-separated value of the pref is the id
  // of the last viewed screen of the feature tour
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.firefox-view.feature-tour",
        '{"message":"FIREFOX_VIEW_FEATURE_TOUR","screen":"FEATURE_CALLOUT_2","complete":false}',
      ],
    ],
  });
  // Open an about:firefoxview tab
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:firefoxview"
  );
  let tab1Doc = tab1.linkedBrowser.contentWindow.document;
  await waitForCalloutRender(tab1Doc);
  await waitForCalloutPositioned(tab1Doc);
  ok(
    tab1Doc.querySelector(".FEATURE_CALLOUT_2"),
    "First tab's Feature Callout shows the tour screen saved in the user pref"
  );

  // Open a second about:firefoxview tab
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:firefoxview"
  );
  let tab2Doc = tab2.linkedBrowser.contentWindow.document;
  await waitForCalloutRender(tab2Doc);
  await waitForCalloutPositioned(tab2Doc);
  ok(
    tab2Doc.querySelector(".FEATURE_CALLOUT_2"),
    "Second tab's Feature Callout shows the tour screen saved in the user pref"
  );

  tab2Doc.querySelector(".action-buttons .primary").click();

  await waitForCalloutScreen(tab1Doc, ".FEATURE_CALLOUT_3");

  ok(
    tab1Doc.querySelector(".FEATURE_CALLOUT_3"),
    "First tab's Feature Callout advances to the next screen when the tour is advanced in second tab"
  );

  tab1Doc.querySelector(".action-buttons .primary").click();

  await BrowserTestUtils.waitForMutationCondition(
    tab1Doc.body,
    { childList: true },
    () => !tab1Doc.body.querySelector(calloutSelector)
  );

  await BrowserTestUtils.waitForMutationCondition(
    tab2Doc.body,
    { childList: true },
    () => !tab2Doc.body.querySelector(calloutSelector)
  );

  ok(
    !tab1Doc.body.querySelector(calloutSelector),
    "Feature Callout is removed in first tab after being dismissed in first tab"
  );
  ok(
    !tab2Doc.body.querySelector(calloutSelector),
    "Feature Callout is removed in second tab after tour was dismissed in first tab"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
