/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

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

const waitForCalloutRemoved = async doc => {
  await BrowserTestUtils.waitForCondition(() => {
    return !doc.body.querySelector(calloutSelector);
  });
};

const clickPrimaryButton = async doc => {
  doc.querySelector(primaryButtonSelector).click();
};

add_task(async function feature_callout_renders_in_firefox_view() {
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

      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );
    }
  );
});

add_task(async function feature_callout_is_not_shown_twice() {
  // Third comma-separated value of the pref is set to a string value once a user completes the tour
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, '{"message":"","screen":"","complete":true}']],
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

add_task(async function feature_callout_syncs_across_visits_and_tabs() {
  // Second comma-separated value of the pref is the id
  // of the last viewed screen of the feature tour
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        featureTourPref,
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
  await waitForCalloutScreen(tab1Doc, 2);

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
  await waitForCalloutScreen(tab2Doc, 2);

  ok(
    tab2Doc.querySelector(".FEATURE_CALLOUT_2"),
    "Second tab's Feature Callout shows the tour screen saved in the user pref"
  );

  await clickPrimaryButton(tab2Doc);
  gBrowser.selectedTab = tab1;
  await waitForCalloutScreen(tab1Doc, 3);

  ok(
    tab1Doc.querySelector(".FEATURE_CALLOUT_3"),
    "First tab's Feature Callout advances to the next screen when the tour is advanced in second tab"
  );

  await clickPrimaryButton(tab1Doc);
  gBrowser.selectedTab = tab1;
  await waitForCalloutRemoved(tab1Doc);

  ok(
    !tab1Doc.body.querySelector(calloutSelector),
    "Feature Callout is removed in first tab after being dismissed in first tab"
  );

  gBrowser.selectedTab = tab2;
  await waitForCalloutRemoved(tab2Doc);

  ok(
    !tab2Doc.body.querySelector(calloutSelector),
    "Feature Callout is removed in second tab after tour was dismissed in first tab"
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

add_task(async function feature_callout_closes_on_dismiss() {
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

      document.querySelector(".dismiss-button").click();
      await waitForCalloutRemoved(document);

      ok(
        !document.querySelector(calloutSelector),
        "Callout is removed from screen on dismiss"
      );

      let tourComplete = JSON.parse(
        Services.prefs.getStringPref(featureTourPref)
      ).complete;
      ok(
        tourComplete,
        `Tour is recorded as complete in ${featureTourPref} preference value`
      );
    }
  );
});

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

add_task(async function feature_callout_only_highlights_existing_elements() {
  // Third comma-separated value of the pref is set to a string value once a user completes the tour
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", getPrefValueByScreen(2)]],
  });

  const sandbox = sinon.createSandbox();
  // Return no active colorways
  sandbox.stub(BuiltInThemes, "findActiveColorwayCollection").returns(false);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, 2);

      ok(
        document
          .querySelector(primaryButtonSelector)
          .getAttribute("data-l10n-id") ===
          "callout-primary-complete-button-label",
        "When parent element for third screen isn't present, second screen has CTA to finish tour"
      );

      // Click to finish tour
      await clickPrimaryButton(document);

      ok(
        !document.querySelector(`${calloutSelector}:not(.hidden)`),
        "Feature Callout screen does not render if its parent element does not exist"
      );

      sandbox.restore();
    }
  );
});

add_task(async function feature_callout_arrow_class_exists() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", defaultPrefValue]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, 1);

      const arrowParent = document.querySelector(".callout-arrow.arrow-top");
      ok(arrowParent, "Arrow class exists on parent container");
    }
  );
});

add_task(async function feature_callout_arrow_is_not_flipped_on_ltr() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.feature-tour", getPrefValueByScreen(3)]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, 3);
      let arrowParent = document.querySelector(
        ".callout-arrow.arrow-inline-end"
      );

      ok(
        arrowParent,
        "Feature Callout arrow parent has arrow-end class when arrow direction is set to 'end'"
      );
    }
  );
});
