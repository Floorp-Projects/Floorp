/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

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

/**
 * wait for a feature callout screen of given parameters to be shown
 * @param {Document} doc the document in which the callout appears
 * @param {String|Number} screenPostfix Number: the screen number to wait for.
 *                                      String: the full ID of the screen.
 */
const waitForCalloutScreen = async (doc, screenPostfix) => {
  await BrowserTestUtils.waitForCondition(() =>
    doc.querySelector(
      `${calloutSelector}:not(.hidden) .${
        typeof screenPostfix === "number" ? "FEATURE_CALLOUT_" : ""
      }${screenPostfix}`
    )
  );
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
  tab1.focus();
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
  tab2.focus();
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
      await BrowserTestUtils.waitForCondition(() => {
        return document.querySelector(
          `${calloutSelector}.arrow-inline-end:not(.hidden)`
        );
      });
      ok(
        true,
        "Feature Callout arrow parent has arrow-end class when arrow direction is set to 'end'"
      );
    }
  );
});

add_task(async function feature_callout_respects_cfr_features_pref() {
  async function toggleCFRFeaturesPref(value, extraPrefs = []) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
          value,
        ],
        ...extraPrefs,
      ],
    });
  }

  await toggleCFRFeaturesPref(true, [[featureTourPref, defaultPrefValue]]);

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

      await toggleCFRFeaturesPref(false);
      await waitForCalloutRemoved(document);
      ok(
        !document.querySelector(calloutSelector),
        "Feature Callout element was removed because CFR pref was disabled"
      );
    }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      ok(
        !document.querySelector(calloutSelector),
        "Feature Callout element was not created because CFR pref was disabled"
      );

      await toggleCFRFeaturesPref(true);
      await waitForCalloutScreen(document, 1);
      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element was created because CFR pref was enabled"
      );
    }
  );
});

add_task(
  async function feature_callout_tab_pickup_reminder_primary_click_elm() {
    await SpecialPowers.pushPrefEnv({
      set: [[featureTourPref, `{"message":"","screen":"","complete":true}`]],
    });
    Services.prefs.setIntPref("browser.firefox-view.view-count", 3);
    Services.prefs.setBoolPref("identity.fxaccounts.enabled", false);
    ASRouter.resetMessageState();
    registerCleanupFunction(() => {
      Services.prefs.clearUserPref("browser.firefox-view.view-count");
      Services.prefs.clearUserPref("identity.fxaccounts.enabled");
      ASRouter.resetMessageState();
    });

    const expectedUrl = await fxAccounts.constructor.config.promiseConnectAccountURI(
      "firefoxview"
    );
    info(`Expected FxA URL: ${expectedUrl}`);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        let tabOpened = new Promise(resolve => {
          gBrowser.tabContainer.addEventListener(
            "TabOpen",
            event => {
              let newTab = event.target;
              let newBrowser = newTab.linkedBrowser;
              let result = newTab;
              BrowserTestUtils.waitForDocLoadAndStopIt(
                expectedUrl,
                newBrowser
              ).then(() => resolve(result));
            },
            { once: true }
          );
        });

        info("Waiting for callout to render");
        await waitForCalloutScreen(
          document,
          "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
        );

        info("Clicking primary button");
        let calloutRemoved = waitForCalloutRemoved(document);
        await clickPrimaryButton(document);
        let openedTab = await tabOpened;
        ok(openedTab, "FxA sign in page opened");
        // The callout should be removed when primary CTA is clicked
        await calloutRemoved;
        BrowserTestUtils.removeTab(openedTab);
      }
    );
    Services.prefs.clearUserPref("browser.firefox-view.view-count");
    Services.prefs.clearUserPref("identity.fxaccounts.enabled");
    ASRouter.resetMessageState();
  }
);
