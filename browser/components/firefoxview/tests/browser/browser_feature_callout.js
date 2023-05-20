/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const { MessageLoaderUtils } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const { BuiltInThemes } = ChromeUtils.importESModule(
  "resource:///modules/BuiltInThemes.sys.mjs"
);

const featureTourPref = "browser.firefox-view.feature-tour";
const defaultPrefValue = getPrefValueByScreen(1);

add_setup(async function () {
  requestLongerTimeout(2);
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
  await waitForCalloutScreen(tab2Doc, "FEATURE_CALLOUT_1");

  ok(
    tab2Doc.querySelector(".FEATURE_CALLOUT_1"),
    "Second tab's Feature Callout shows the tour screen saved in the user pref"
  );

  await clickPrimaryButton(tab2Doc);

  gBrowser.selectedTab = tab1;
  tab1.focus();
  await waitForCalloutScreen(tab1Doc, "FEATURE_CALLOUT_2");
  ok(
    tab1Doc.querySelector(".FEATURE_CALLOUT_2"),
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
  Services.prefs.clearUserPref(featureTourPref);
});

add_task(async function feature_callout_closes_on_dismiss() {
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_2_NO_CWS"
  );
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  const spy = new TelemetrySpy(sandbox);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

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

add_task(async function feature_callout_not_rendered_when_it_has_no_parent() {
  Services.telemetry.clearEvents();
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  testMessage.message.content.screens[0].parent_selector = "#fake-selector";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      const CONTAINER_NOT_CREATED_EVENT = [
        [
          "messaging_experiments",
          "feature_callout",
          "create_failed",
          `${testMessage.message.id}-${testMessage.message.content.screens[0].parent_selector}`,
        ],
      ];
      await TestUtils.waitForCondition(() => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        return events && events.length >= 2;
      }, "Waiting for container_not_created event");

      TelemetryTestUtils.assertEvents(
        CONTAINER_NOT_CREATED_EVENT,
        { method: "feature_callout" },
        { clear: true, process: "parent" }
      );

      ok(
        !document.querySelector(`${calloutSelector}:not(.hidden)`),
        "Feature Callout screen does not render if its parent element does not exist"
      );
    }
  );

  sandbox.restore();
});

add_task(async function feature_callout_only_highlights_existing_elements() {
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  testMessage.message.content.screens[0].parent_selector = "#fake-selector";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        !document.querySelector(`${calloutSelector}:not(.hidden)`),
        "Feature Callout screen does not render if its parent element does not exist"
      );
    }
  );
  sandbox.restore();
});

add_task(async function feature_callout_arrow_class_exists() {
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

      const arrowParent = document.querySelector(".callout-arrow.arrow-top");
      ok(arrowParent, "Arrow class exists on parent container");
    }
  );
  sandbox.restore();
});

add_task(async function feature_callout_arrow_is_not_flipped_on_ltr() {
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  testMessage.message.content.screens[0].content.arrow_position = "start";
  testMessage.message.content.screens[0].parent_selector = "span.brand-icon";
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      await BrowserTestUtils.waitForCondition(() => {
        return document.querySelector(
          `${calloutSelector}.arrow-inline-start:not(.hidden)`
        );
      });
      ok(
        true,
        "Feature Callout arrow parent has arrow-start class when arrow direction is set to 'start'"
      );
    }
  );
  sandbox.restore();
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

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
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
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element was created because CFR pref was enabled"
      );
    }
  );
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
    Services.prefs.clearUserPref("identity.fxaccounts.enabled");
    sandbox.restore();
  }
);

add_task(async function feature_callout_dismiss_on_page_click() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, `{"message":"","screen":"","complete":true}`]],
  });
  const screenId = "FIREFOX_VIEW_TAB_PICKUP_REMINDER";
  const testClickSelector = "#recently-closed-tabs-container";
  let testMessage = getCalloutMessageById(screenId);
  // Configure message with a dismiss action on tab container click
  testMessage.message.content.screens[0].content.page_event_listeners = [
    {
      params: {
        type: "click",
        selectors: testClickSelector,
      },
      action: {
        dismiss: true,
      },
    },
  ];
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  const spy = new TelemetrySpy(sandbox);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      info("Waiting for callout to render");
      await waitForCalloutScreen(document, screenId);

      info("Clicking page element");
      document.querySelector(testClickSelector).click();
      await waitForCalloutRemoved(document);

      // Test that appropriate telemetry is sent
      spy.assertCalledWith({
        event: "PAGE_EVENT",
        event_context: {
          action: "DISMISS",
          reason: "CLICK",
          source: sinon.match(testClickSelector),
          page: "about:firefoxview",
        },
        message_id: screenId,
      });
      spy.assertCalledWith({
        event: "DISMISS",
        event_context: {
          source: sinon
            .match("PAGE_EVENT:")
            .and(sinon.match(testClickSelector)),
          page: "about:firefoxview",
        },
        message_id: screenId,
      });

      browser.tabDialogBox
        ?.getTabDialogManager()
        .dialogs.forEach(dialog => dialog.close());
    }
  );
  Services.prefs.clearUserPref("browser.firefox-view.view-count");
  sandbox.restore();
  ASRouter.resetMessageState();
});

add_task(async function feature_callout_advance_tour_on_page_click() {
  let sandbox = sinon.createSandbox();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        featureTourPref,
        JSON.stringify({
          message: "FIREFOX_VIEW_FEATURE_TOUR",
          screen: "FEATURE_CALLOUT_1",
          complete: false,
        }),
      ],
    ],
  });

  // Add page action listeners to the built-in messages.
  const TEST_MESSAGES = FeatureCalloutMessages.getMessages().filter(msg =>
    [
      "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS",
      "FIREFOX_VIEW_FEATURE_TOUR_2_NO_CWS",
    ].includes(msg.id)
  );
  TEST_MESSAGES.forEach(msg => {
    let { content } = msg.content.screens[msg.content.startScreen ?? 0];
    content.page_event_listeners = [
      {
        params: {
          type: "click",
          selectors: ".brand-logo",
        },
        action: JSON.parse(JSON.stringify(content.primary_button.action)),
      },
    ];
  });
  const getMessagesStub = sandbox.stub(FeatureCalloutMessages, "getMessages");
  getMessagesStub.returns(TEST_MESSAGES);
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      info("Clicking page button");
      document.querySelector(".brand-logo").click();

      await waitForCalloutScreen(document, "FEATURE_CALLOUT_2");
      info("Clicking page button");
      document.querySelector(".brand-logo").click();

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
  await ASRouter._updateMessageProviders();
  await ASRouter.loadMessagesFromAllProviders(
    ASRouter.state.providers.filter(p => p.id === "onboarding")
  );
});

add_task(async function feature_callout_dismiss_on_escape() {
  await SpecialPowers.pushPrefEnv({
    set: [[featureTourPref, `{"message":"","screen":"","complete":true}`]],
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
      info("Waiting for the Fx View Spotlight promo to open");
      let dialogBrowser = await dialogOpenPromise;
      let primaryBtnSelector = ".action-buttons button.primary";
      await TestUtils.waitForCondition(
        () => dialogBrowser.document.querySelector("main.DEFAULT_MODAL_UI"),
        `Should render main.DEFAULT_MODAL_UI`
      );

      dialogBrowser.document.querySelector(primaryBtnSelector).click();
      info("Fx View Spotlight promo clicked");

      await BrowserTestUtils.waitForCondition(
        () =>
          browser.contentWindow.performance.navigation.type ==
          browser.contentWindow.performance.navigation.TYPE_RELOAD
      );
      info("Spotlight modal cleared, entering feature tour");

      const { document } = browser.contentWindow;
      await waitForCalloutScreen(document, "FEATURE_CALLOUT_1");
      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );
      info("Feature tour started");
      await clickPrimaryButton(document);
    }
  );

  ok(remoteSettingsStub.called, "Tried to load CFR messages");
  sandbox.restore();
  ASRouter.resetMessageState();
});

add_task(async function feature_callout_returns_default_fxview_focus_to_top() {
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

      ok(
        document.querySelector(calloutSelector),
        "Feature Callout element exists"
      );

      document.querySelector(".dismiss-button").click();
      await waitForCalloutRemoved(document);

      ok(
        document.activeElement.localName === "body",
        "by default focus returns to the document body after callout closes"
      );
    }
  );
  sandbox.restore();
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
        ok(
          document.activeElement.id === "recently-closed-tabs-header-section",
          "when focus changes away from callout it reverts after callout closes"
        );
      }
    );
    sandbox.restore();
  }
);

add_task(async function feature_callout_does_not_display_arrow_if_hidden() {
  const testMessage = getCalloutMessageById(
    "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS"
  );
  testMessage.message.content.screens[0].content.hide_arrow = true;
  const sandbox = createSandboxWithCalloutTriggerStub(testMessage);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      ok(
        getComputedStyle(
          document.querySelector(".callout-arrow"),
          ":before"
        ).getPropertyValue("display") == "none" &&
          getComputedStyle(
            document.querySelector(".callout-arrow"),
            ":after"
          ).getPropertyValue("display") == "none",
        "callout arrow is not visible"
      );
    }
  );
  sandbox.restore();
});
