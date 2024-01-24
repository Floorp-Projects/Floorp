/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const { MessageLoaderUtils } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const { BuiltInThemes } = ChromeUtils.importESModule(
  "resource:///modules/BuiltInThemes.sys.mjs"
);

const defaultPrefValue = getPrefValueByScreen(1);

add_setup(async function () {
  requestLongerTimeout(3);
  registerCleanupFunction(() => ASRouter.resetMessageState());
});

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

      launchFeatureTourIn(browser.contentWindow);
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");

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
    set: [[featureTourPref, '{"screen":"","complete":true}']],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      ok(
        !document.querySelector(calloutSelector),
        "Feature Callout tour does not render if the user finished it previously"
      );
    }
  );
  Services.prefs.clearUserPref(featureTourPref);
});

add_task(async function feature_callout_syncs_across_visits_and_tabs() {
  // Second comma-separated value of the pref is the id
  // of the last viewed screen of the feature tour
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, '{"screen":"FEATURE_CALLOUT_1","complete":false}']],
  });
  // Open an about:firefoxview tab
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:firefoxview"
  );
  let tab1Doc = tab1.linkedBrowser.contentWindow.document;
  launchFeatureTourIn(tab1.linkedBrowser.contentWindow);
  await waitForCalloutScreen(tab1Doc, "FEATURE_CALLOUT_1");

  ok(
    tab1Doc.querySelector(".FEATURE_CALLOUT_1"),
    "First tab's Feature Callout shows the tour screen saved in the user pref"
  );

  // Open a second about:firefoxview tab
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:firefoxview"
  );
  let tab2Doc = tab2.linkedBrowser.contentWindow.document;
  launchFeatureTourIn(tab2.linkedBrowser.contentWindow);
  await waitForCalloutScreen(tab2Doc, "FEATURE_CALLOUT_1");

  ok(
    tab2Doc.querySelector(".FEATURE_CALLOUT_1"),
    "Second tab's Feature Callout shows the tour screen saved in the user pref"
  );

  await clickCTA(tab2Doc);
  await waitForCalloutScreen(tab2Doc, "FEATURE_CALLOUT_2");

  gBrowser.selectedTab = tab1;
  tab1.focus();
  await waitForCalloutScreen(tab1Doc, "FEATURE_CALLOUT_2");
  ok(
    tab1Doc.querySelector(".FEATURE_CALLOUT_2"),
    "First tab's Feature Callout advances to the next screen when the tour is advanced in second tab"
  );

  await clickCTA(tab1Doc);
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
  Services.prefs.clearUserPref(featureTourPref);
});

add_task(async function feature_callout_closes_on_dismiss() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, '{"screen":"FEATURE_CALLOUT_2","complete":false}']],
  });
  const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  const spy = new TelemetrySpy(sandbox);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");

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

      // Test that appropriate telemetry is sent
      spy.assertCalledWith({
        event: "CLICK_BUTTON",
        event_context: {
          source: "dismiss_button",
          page: "about:firefoxview",
        },
        message_id: sinon.match("FEATURE_CALLOUT_2"),
      });
      spy.assertCalledWith({
        event: "DISMISS",
        event_context: {
          source: "dismiss_button",
          page: "about:firefoxview",
        },
        message_id: sinon.match("FEATURE_CALLOUT_2"),
      });
    }
  );
  sandbox.restore();
});

add_task(async function feature_callout_arrow_position_attribute_exists() {
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

      const callout = await BrowserTestUtils.waitForCondition(
        () =>
          document.querySelector(`${calloutSelector}[arrow-position="top"]`),
        "Waiting for callout to render"
      );
      is(
        callout.getAttribute("arrow-position"),
        "top",
        "Arrow position attribute exists on parent container"
      );
    }
  );
  sandbox.restore();
});

add_task(async function feature_callout_arrow_is_not_flipped_on_ltr() {
  const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  testMessage.message.content.screens[0].anchors[0].arrow_position = "start";
  testMessage.message.content.screens[0].anchors[0].selector =
    "span.brand-feature-name";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      const callout = await BrowserTestUtils.waitForCondition(
        () =>
          document.querySelector(
            `${calloutSelector}[arrow-position="inline-start"]:not(.hidden)`
          ),
        "Waiting for callout to render"
      );
      is(
        callout.getAttribute("arrow-position"),
        "inline-start",
        "Feature callout has inline-start arrow position when arrow_position is set to 'start'"
      );
      ok(
        !callout.classList.contains("hidden"),
        "Feature Callout is not hidden"
      );
    }
  );
  sandbox.restore();
});

add_task(async function feature_callout_respects_cfr_features_pref() {
  async function toggleCFRFeaturesPref(value) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
          value,
        ],
      ],
    });
  }

  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, defaultPrefValue]],
  });

  await toggleCFRFeaturesPref(true);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
  await toggleCFRFeaturesPref(false);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      ok(
        !document.querySelector(calloutSelector),
        "Feature Callout element was not created because CFR pref was disabled"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});

add_task(
  async function feature_callout_tab_pickup_reminder_primary_click_elm() {
    Services.prefs.setBoolPref("identity.fxaccounts.enabled", false);
    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
    );
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    const expectedUrl =
      await fxAccounts.constructor.config.promiseConnectAccountURI("fx-view");
    info(`Expected FxA URL: ${expectedUrl}`);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

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
        await clickCTA(document);
        let openedTab = await tabOpened;
        ok(openedTab, "FxA sign in page opened");
        // The callout should be removed when primary CTA is clicked
        await calloutRemoved;
        BrowserTestUtils.removeTab(openedTab);
      }
    );
    Services.prefs.clearUserPref("identity.fxaccounts.enabled");
    sandbox.restore();
  }
);

add_task(async function feature_callout_dismiss_on_timeout() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, `{"screen":"","complete":true}`]],
  });
  const screenId = "FIREFOX_VIEW_TAB_PICKUP_REMINDER";
  let testMessage = getCalloutMessageById(screenId);
  // Configure message with a dismiss action on tab container click
  testMessage.message.content.screens[0].content.page_event_listeners = [
    {
      params: { type: "timeout", options: { once: true, interval: 5000 } },
      action: { dismiss: true, type: "CANCEL" },
    },
  ];
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  const telemetrySpy = new TelemetrySpy(sandbox);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      let onInterval;
      let startedInterval = new Promise(resolve => {
        sandbox
          .stub(browser.contentWindow, "setInterval")
          .callsFake((fn, ms) => {
            Assert.strictEqual(
              ms,
              5000,
              "setInterval called with 5 second interval"
            );
            onInterval = fn;
            resolve();
            return 1;
          });
      });

      launchFeatureTourIn(browser.contentWindow);

      info("Waiting for callout to render");
      await startedInterval;
      await waitForCalloutScreen(document, screenId);

      info("Ending timeout");
      onInterval();
      await waitForCalloutRemoved(document);

      // Test that appropriate telemetry is sent
      telemetrySpy.assertCalledWith({
        event: "PAGE_EVENT",
        event_context: {
          action: "CANCEL",
          reason: "TIMEOUT",
          source: "timeout",
          page: "about:firefoxview",
        },
        message_id: screenId,
      });
      telemetrySpy.assertCalledWith({
        event: "DISMISS",
        event_context: {
          source: "PAGE_EVENT:timeout",
          page: "about:firefoxview",
        },
        message_id: screenId,
      });
    }
  );
  Services.prefs.clearUserPref("browser.firefox-view.view-count");
  sandbox.restore();
  ASRouter.resetMessageState();
});

add_task(async function feature_callout_advance_tour_on_page_click() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        featureTourPref,
        JSON.stringify({
          screen: "FEATURE_CALLOUT_1",
          complete: false,
        }),
      ],
    ],
  });

  // Add page action listeners to the built-in messages.
  let testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  // Configure message with a dismiss action on tab container click
  testMessage.message.content.screens.forEach(screen => {
    screen.content.page_event_listeners = [
      {
        params: { type: "click", selectors: ".brand-logo" },
        action: JSON.parse(
          JSON.stringify(screen.content.primary_button.action)
        ),
      },
    ];
  });
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
      info("Clicking page container");
      // We intentionally turn off a11y_checks, because the following click
      // is send to dismiss the feature callout using an alternative way of
      // the callout dismissal, where other ways are accessible, therefore
      // this test can be ignored.
      AccessibilityUtils.setEnv({
        mustHaveAccessibleRule: false,
      });
      document.querySelector(".brand-logo").click();
      AccessibilityUtils.resetEnv();

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
      info("Clicking page container");
      // We intentionally turn off a11y_checks, because the following click
      // is send to dismiss the feature callout using an alternative way of
      // the callout dismissal, where other ways are accessible, therefore
      // this test can be ignored.
      AccessibilityUtils.setEnv({
        mustHaveAccessibleRule: false,
      });
      document.querySelector(".brand-logo").click();
      AccessibilityUtils.resetEnv();

      await waitForCalloutRemoved(document);
      let tourComplete = JSON.parse(
        Services.prefs.getStringPref(featureTourPref)
      ).complete;
      ok(
        tourComplete,
        `Tour is recorded as complete in ${featureTourPref} preference value`
      );
    }
  );

  sandbox.restore();
  ASRouter.resetMessageState();
});

add_task(async function feature_callout_dismiss_on_escape() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, `{"screen":"","complete":true}`]],
  });
  const screenId = "FIREFOX_VIEW_TAB_PICKUP_REMINDER";
  let testMessage = getCalloutMessageById(screenId);
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  const spy = new TelemetrySpy(sandbox);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      launchFeatureTourIn(browser.contentWindow);

      info("Waiting for callout to render");
      await waitForCalloutScreen(document, screenId);

      info("Pressing escape");
      // Press Escape to close
      EventUtils.synthesizeKey("KEY_Escape", {}, browser.contentWindow);
      await waitForCalloutRemoved(document);

      // Test that appropriate telemetry is sent
      spy.assertCalledWith({
        event: "DISMISS",
        event_context: {
          source: "KEY_Escape",
          page: "about:firefoxview",
        },
        message_id: screenId,
      });
    }
  );
  Services.prefs.clearUserPref("browser.firefox-view.view-count");
  sandbox.restore();
  ASRouter.resetMessageState();
});

add_task(async function test_firefox_view_spotlight_promo() {
  // Prevent attempts to fetch CFR messages remotely.
  const sandbox = sinon.createSandbox();
  let remoteSettingsStub = sandbox.stub(
    MessageLoaderUtils,
    "_remoteSettingsLoader"
  );
  remoteSettingsStub.resolves([]);

  await SpecialPowers.pushPrefEnv({
    clear: [
      [featureTourPref],
      ["browser.newtabpage.activity-stream.asrouter.providers.cfr"],
    ],
  });
  ASRouter.resetMessageState();

  let dialogOpenPromise = BrowserTestUtils.promiseAlertDialogOpen(
    null,
    "chrome://browser/content/spotlight.html",
    { isSubDialog: true }
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      launchFeatureTourIn(browser.contentWindow);

      info("Waiting for the Fx View Spotlight promo to open");
      let dialogBrowser = await dialogOpenPromise;
      let primaryBtnSelector = ".action-buttons button.primary";
      await TestUtils.waitForCondition(
        () => dialogBrowser.document.querySelector("main.DEFAULT_MODAL_UI"),
        `Should render main.DEFAULT_MODAL_UI`
      );

      dialogBrowser.document.querySelector(primaryBtnSelector).click();
      info("Fx View Spotlight promo clicked");

      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );
      info("Feature tour started");
      await clickCTA(document);
    }
  );

  ok(remoteSettingsStub.called, "Tried to load CFR messages");
  sandbox.restore();
  await SpecialPowers.popPrefEnv();
  ASRouter.resetMessageState();
});

add_task(async function feature_callout_returns_default_fxview_focus_to_top() {
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

      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );

      document.querySelector(".dismiss-button").click();
      await waitForCalloutRemoved(document);

      Assert.strictEqual(
        document.activeElement.localName,
        "body",
        "by default focus returns to the document body after callout closes"
      );
    }
  );
  sandbox.restore();
  await SpecialPowers.popPrefEnv();
  ASRouter.resetMessageState();
});

add_task(
  async function feature_callout_returns_moved_fxview_focus_to_previous() {
    const testMessage = getCalloutMessageById(
      "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
    );
    const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;

        launchFeatureTourIn(browser.contentWindow);

        await waitForCalloutScreen(
          document,
          "FIREFOX_VIEW_TAB_PICKUP_REMINDER"
        );

        // change focus to recently-closed-tabs-container
        let recentlyClosedHeaderSection = document.querySelector(
          "#recently-closed-tabs-header-section"
        );
        recentlyClosedHeaderSection.focus();

        // close the callout dialog
        document.querySelector(".dismiss-button").click();
        await waitForCalloutRemoved(document);

        // verify that the focus landed in the right place
        Assert.strictEqual(
          document.activeElement.id,
          "recently-closed-tabs-header-section",
          "when focus changes away from callout it reverts after callout closes"
        );
      }
    );
    sandbox.restore();
  }
);

add_task(async function feature_callout_does_not_display_arrow_if_hidden() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, defaultPrefValue]],
  });
  const testMessage = getCalloutMessageById("FIREFOX_VIEW_FEATURE_TOUR");
  testMessage.message.content.screens[0].anchors[0].hide_arrow = true;
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

      is(
        getComputedStyle(
          document.querySelector(`${calloutSelector} .arrow-box`)
        ).getPropertyValue("display"),
        "none",
        "callout arrow is not visible"
      );
    }
  );
  sandbox.restore();
});
