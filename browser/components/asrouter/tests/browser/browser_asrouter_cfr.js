/* eslint-disable @microsoft/sdl/no-insecure-url */
const { ASRouterTriggerListeners } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterTriggerListeners.sys.mjs"
);
const { ASRouter } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouter.sys.mjs"
);
const { CFRMessageProvider } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/CFRMessageProvider.sys.mjs"
);

const { TelemetryFeed } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/TelemetryFeed.sys.mjs"
);

const createDummyRecommendation = ({
  action,
  category,
  heading_text,
  layout,
  skip_address_bar_notifier,
  show_in_private_browsing,
  template,
}) => {
  let recommendation = {
    template,
    groups: ["mochitest-group"],
    content: {
      layout: layout || "addon_recommendation",
      category,
      anchor_id: "page-action-buttons",
      skip_address_bar_notifier,
      show_in_private_browsing,
      heading_text: heading_text || "Mochitest",
      info_icon: {
        label: { attributes: { tooltiptext: "Why am I seeing this" } },
        sumo_path: "extensionrecommendations",
      },
      icon: "chrome://activity-stream/content/data/content/assets/glyph-webextension-16.svg",
      icon_dark_theme:
        "chrome://activity-stream/content/data/content/assets/glyph-webextension-16.svg",
      learn_more: "extensionrecommendations",
      addon: {
        id: "addon-id",
        title: "Addon name",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        author: "Author name",
        amo_url: "https://example.com",
      },
      descriptionDetails: { steps: [] },
      text: "Mochitest",
      buttons: {
        primary: {
          label: {
            value: "OK",
            attributes: { accesskey: "O" },
          },
          action: {
            type: action.type,
            data: {},
          },
        },
        secondary: [
          {
            label: {
              value: "Cancel",
              attributes: { accesskey: "C" },
            },
            action: {
              type: "CANCEL",
            },
          },
          {
            label: {
              value: "Cancel 1",
              attributes: { accesskey: "A" },
            },
          },
          {
            label: {
              value: "Cancel 2",
              attributes: { accesskey: "B" },
            },
          },
        ],
      },
    },
  };
  recommendation.content.notification_text = new String("Mochitest"); // eslint-disable-line
  recommendation.content.notification_text.attributes = {
    tooltiptext: "Mochitest tooltip",
    "a11y-announcement": "Mochitest announcement",
  };
  return recommendation;
};

function checkCFRAddonsElements(notification) {
  Assert.ok(notification.hidden === false, "Panel should be visible");
  Assert.equal(
    notification.getAttribute("data-notification-category"),
    "addon_recommendation",
    "Panel have correct data attribute"
  );
  Assert.ok(
    notification.querySelector("#cfr-notification-footer-text-and-addon-info"),
    "Panel should have addon info container"
  );
  Assert.ok(
    notification.querySelector("#cfr-notification-footer-filled-stars"),
    "Panel should have addon rating info"
  );
  Assert.ok(
    notification.querySelector("#cfr-notification-author"),
    "Panel should have author info"
  );
}

function clearNotifications() {
  for (let notification of PopupNotifications._currentNotifications) {
    notification.remove();
  }

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );
}

function trigger_cfr_panel(
  browser,
  trigger,
  {
    action = { type: "CANCEL" },
    heading_text,
    category = "cfrAddons",
    layout,
    skip_address_bar_notifier = false,
    use_single_secondary_button = false,
    show_in_private_browsing = false,
    template = "cfr_doorhanger",
  } = {}
) {
  // a fake action type will result in the action being ignored
  const recommendation = createDummyRecommendation({
    action,
    category,
    heading_text,
    layout,
    skip_address_bar_notifier,
    show_in_private_browsing,
    template,
  });
  if (category !== "cfrAddons") {
    delete recommendation.content.addon;
  }
  if (use_single_secondary_button) {
    recommendation.content.buttons.secondary = [
      recommendation.content.buttons.secondary[0],
    ];
  }

  clearNotifications();
  return CFRPageActions.addRecommendation(
    browser,
    trigger,
    recommendation,
    // Use the real AS dispatch method to trigger real notifications
    ASRouter.dispatchCFRAction
  );
}

add_setup(async function () {
  // Store it in order to restore to the original value
  const { _fetchLatestAddonVersion } = CFRPageActions;
  // Prevent fetching the real addon url and making a network request
  CFRPageActions._fetchLatestAddonVersion = () => "http://example.com";
  Services.fog.testResetFOG();

  registerCleanupFunction(() => {
    CFRPageActions._fetchLatestAddonVersion = _fetchLatestAddonVersion;
    clearNotifications();
    CFRPageActions.clearRecommendations();
  });
});

add_task(async function test_cfr_notification_show() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.telemetry", true]],
  });

  let pingSubmitted = false;
  GleanPings.messagingSystem.testBeforeNextSubmit(() => {
    pingSubmitted = true;
    Assert.equal(Glean.messagingSystem.source.testGetValue(), "CFR");
  });

  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  const response = await trigger_cfr_panel(browser, "example.com");
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  const oldFocus = document.activeElement;
  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .hidden === false,
    "Panel should be visible"
  );
  Assert.equal(
    document.activeElement,
    oldFocus,
    "Focus didn't move when panel was shown"
  );

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .button
  );
  let hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );

  Assert.ok(pingSubmitted, "Recorded an event");
  Services.fog.testResetFOG();
});

add_task(async function test_cfr_notification_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  let response = await trigger_cfr_panel(browser, "example.com", {
    heading_text: "First Message",
  });
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );
  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  // Try adding another message
  response = await trigger_cfr_panel(browser, "example.com", {
    heading_text: "Second Message",
  });
  Assert.equal(
    response,
    false,
    "Should return false if second call did not add the message"
  );

  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .hidden === false,
    "Panel should be visible"
  );

  Assert.equal(
    document.getElementById("cfr-notification-header-label").value,
    "First Message",
    "The first message should be visible"
  );

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .button
  );
  let hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );
  Services.fog.testResetFOG();
});

add_task(async function test_cfr_notification_minimize() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  let response = await trigger_cfr_panel(browser, "example.com");
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  await BrowserTestUtils.waitForCondition(
    () => gURLBar.hasAttribute("cfr-recommendation-state"),
    "Wait for the notification to show up and have a state"
  );
  Assert.ok(
    gURLBar.getAttribute("cfr-recommendation-state") === "expanded",
    "CFR recomendation state is correct"
  );

  gURLBar.focus();

  await BrowserTestUtils.waitForCondition(
    () => gURLBar.getAttribute("cfr-recommendation-state") === "collapsed",
    "After urlbar focus the CFR notification should collapse"
  );

  // Open the panel and click to dismiss to ensure cleanup
  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  let hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;
  Services.fog.testResetFOG();
});

add_task(async function test_cfr_notification_minimize_2() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  let response = await trigger_cfr_panel(browser, "example.com");
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  await BrowserTestUtils.waitForCondition(
    () => gURLBar.hasAttribute("cfr-recommendation-state"),
    "Wait for the notification to show up and have a state"
  );
  Assert.ok(
    gURLBar.getAttribute("cfr-recommendation-state") === "expanded",
    "CFR recomendation state is correct"
  );

  // Open the panel and click to dismiss to ensure cleanup
  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  let hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .secondaryButton,
    "There should be a cancel button"
  );

  // Click the Not Now button
  document
    .getElementById("contextual-feature-recommendation-notification")
    .secondaryButton.click();

  await hidePanel;

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification"),
    "The notification should not dissapear"
  );

  await BrowserTestUtils.waitForCondition(
    () => gURLBar.getAttribute("cfr-recommendation-state") === "collapsed",
    "Clicking the secondary button should collapse the notification"
  );

  clearNotifications();
  CFRPageActions.clearRecommendations();
  Services.fog.testResetFOG();
});

add_task(async function test_cfr_addon_install() {
  // addRecommendation checks that scheme starts with http and host matches
  const browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  const response = await trigger_cfr_panel(browser, "example.com", {
    action: { type: "INSTALL_ADDON_FROM_URL" },
  });
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .hidden === false,
    "Panel should be visible"
  );
  checkCFRAddonsElements(
    document.getElementById("contextual-feature-recommendation-notification")
  );

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .button
  );
  const hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;

  await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

  let [notification] = PopupNotifications.panel.childNodes;
  // Trying to install the addon will trigger a progress popup or an error popup if
  // running the test multiple times in a row
  Assert.ok(
    notification.id === "addon-progress-notification" ||
      notification.id === "addon-install-failed-notification",
    "Should try to install the addon"
  );

  clearNotifications();
  Services.fog.testResetFOG();
});

add_task(async function test_cfr_addon_and_features_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  // Trigger Feature CFR
  let response = await trigger_cfr_panel(browser, "example.com");
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  let showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  const notification = document.getElementById(
    "contextual-feature-recommendation-notification"
  );
  checkCFRAddonsElements(notification);

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(notification.button);
  let hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );

  // Trigger Addon CFR
  response = await trigger_cfr_panel(browser, "example.com", {
    action: { type: "PIN_CURRENT_TAB" },
    category: "cfrAddons",
  });
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  Assert.ok(
    document.getElementById("contextual-feature-recommendation-notification")
      .hidden === false,
    "Panel should be visible"
  );
  checkCFRAddonsElements(
    document.getElementById("contextual-feature-recommendation-notification")
  );

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(notification.button);
  hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );
  Services.fog.testResetFOG();
});

add_task(async function test_onLocationChange_cb() {
  let count = 0;
  const triggerHandler = () => ++count;
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/blue_page.html";
  const browser = gBrowser.selectedBrowser;

  await ASRouterTriggerListeners.get("openURL").init(triggerHandler, [
    "example.com",
  ]);

  BrowserTestUtils.startLoadingURIString(browser, "about:blank");
  await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  Assert.equal(count, 1, "Count navigation to example.com");

  // Anchor scroll triggers a location change event with the same document
  // https://searchfox.org/mozilla-central/rev/8848b9741fc4ee4e9bc3ae83ea0fc048da39979f/uriloader/base/nsIWebProgressListener.idl#400-403
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/#foo");
  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "http://example.com/#foo"
  );

  Assert.equal(count, 1, "It should ignore same page navigation");

  BrowserTestUtils.startLoadingURIString(browser, TEST_URL);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL);

  Assert.equal(count, 2, "We moved to a new document");

  ASRouterTriggerListeners.get("openURL").uninit();
  Services.fog.testResetFOG();
});

add_task(async function test_matchPattern() {
  let count = 0;
  const triggerHandler = () => ++count;
  const frequentVisitsTrigger = ASRouterTriggerListeners.get("frequentVisits");
  await frequentVisitsTrigger.init(triggerHandler, [], ["*://*.example.com/"]);

  const browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "Registered pattern matched the current location"
  );

  BrowserTestUtils.startLoadingURIString(browser, "about:config");
  await BrowserTestUtils.browserLoaded(browser, false, "about:config");

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "Navigated to a new page but not a match"
  );

  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "Navigated to a location that matches the pattern but within 15 mins"
  );

  BrowserTestUtils.startLoadingURIString(browser, "http://www.example.com/");
  await BrowserTestUtils.browserLoaded(
    browser,
    false,
    "http://www.example.com/"
  );

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("www.example.com").length === 1,
    "www.example.com is a different host that also matches the pattern."
  );
  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "www.example.com is a different host that also matches the pattern."
  );

  ASRouterTriggerListeners.get("frequentVisits").uninit();
  Services.fog.testResetFOG();
});

add_task(async function test_providerNames() {
  const providersBranch =
    "browser.newtabpage.activity-stream.asrouter.providers.";
  const cfrProviderPrefs = Services.prefs.getChildList(providersBranch);
  for (const prefName of cfrProviderPrefs) {
    const prefValue = JSON.parse(Services.prefs.getStringPref(prefName));
    if (prefValue?.id) {
      Assert.equal(
        prefValue.id,
        prefName.slice(providersBranch.length),
        "Provider id and pref name should match"
      );
    }
  }
});

add_task(function test_updateCycleForProviders() {
  Services.prefs
    .getChildList("browser.newtabpage.activity-stream.asrouter.providers.")
    .forEach(provider => {
      const prefValue = JSON.parse(Services.prefs.getStringPref(provider, ""));
      if (prefValue?.type === "remote-settings") {
        is(
          typeof prefValue.updateCycleInMs,
          "number",
          "updateCycleInMs is set"
        );
      }
    });
});

add_task(async function test_heartbeat_tactic_2() {
  clearNotifications();

  const msg = (await CFRMessageProvider.getMessages()).find(
    m => m.id === "HEARTBEAT_TACTIC_2"
  );
  const shown = await CFRPageActions.addRecommendation(
    gBrowser.selectedBrowser,
    null,
    {
      ...msg,
      id: `HEARTBEAT_MOCHITEST_${Date.now()}`,
      groups: ["mochitest-group"],
      targeting: true,
    },
    // Use the real AS dispatch method to trigger real notifications
    ASRouter.dispatchCFRAction
  );

  Assert.ok(shown, "Heartbeat CFR added");

  // Wait for visibility change
  BrowserTestUtils.waitForCondition(
    () => document.getElementById("contextual-feature-recommendation"),
    "Heartbeat button exists"
  );

  let newTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    Services.urlFormatter.formatURL(msg.content.action.url),
    true
  );

  document.getElementById("contextual-feature-recommendation").click();

  await newTabPromise;
  gBrowser.removeCurrentTab();
  clearNotifications();
  Services.fog.testResetFOG();
});

add_task(async function test_cfr_doorhanger_in_private_window() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.telemetry", true]],
  });

  let pingSubmitted = false;
  GleanPings.messagingSystem.testBeforeNextSubmit(() => {
    pingSubmitted = true;
    Assert.equal(Glean.messagingSystem.source.testGetValue(), "CFR");
    Assert.equal(
      Glean.messagingSystem.messageId.testGetValue(),
      "n/a",
      "Omitted message_id consistent with CFR telemetry policy"
    );
    Assert.equal(
      Glean.messagingSystem.clientId.testGetValue(),
      undefined,
      "Omitted client_id consistent with CFR telemetry policy"
    );
  });

  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    "http://example.com/"
  );
  const browser = tab.linkedBrowser;

  const response1 = await trigger_cfr_panel(browser, "example.com");
  Assert.ok(
    !response1,
    "CFR should not be shown in a private window if show_in_private_browsing is false"
  );

  const response2 = await trigger_cfr_panel(browser, "example.com", {
    show_in_private_browsing: true,
  });
  Assert.ok(
    response2,
    "CFR should be shown in a private window if show_in_private_browsing is true"
  );

  const shownPromise = BrowserTestUtils.waitForEvent(
    win.PopupNotifications.panel,
    "popupshown"
  );
  win.document.getElementById("contextual-feature-recommendation").click();
  await shownPromise;

  const hiddenPromise = BrowserTestUtils.waitForEvent(
    win.PopupNotifications.panel,
    "popuphidden"
  );
  const button = win.document.getElementById(
    "contextual-feature-recommendation-notification"
  )?.button;
  Assert.ok(button, "CFR doorhanger button found");
  button.click();
  await hiddenPromise;

  Assert.ok(pingSubmitted, "Submitted a CFR messaging system ping");
  await BrowserTestUtils.closeWindow(win);
  Services.fog.testResetFOG();
});
