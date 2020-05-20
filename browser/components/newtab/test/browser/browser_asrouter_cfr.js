const { CFRPageActions } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRPageActions.jsm"
);
const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);

const createDummyRecommendation = ({
  action,
  category,
  heading_text,
  layout,
  skip_address_bar_notifier,
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
      heading_text: heading_text || "Mochitest",
      info_icon: {
        label: { attributes: { tooltiptext: "Why am I seeing this" } },
        sumo_path: "extensionrecommendations",
      },
      icon: "foo",
      icon_dark_theme: "bar",
      learn_more: "extensionrecommendations",
      addon: {
        id: "addon-id",
        title: "Addon name",
        icon: "foo",
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

function checkCFRFeaturesElements(notification) {
  Assert.ok(notification.hidden === false, "Panel should be visible");
  Assert.equal(
    notification.getAttribute("data-notification-category"),
    "message_and_animation",
    "Panel have correct data attribute"
  );
  Assert.ok(
    notification.querySelector(
      "#cfr-notification-footer-pintab-animation-container"
    ),
    "Pin tab animation exists"
  );
  Assert.ok(
    notification.querySelector("#cfr-notification-feature-steps"),
    "Pin tab steps"
  );
}

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

function checkCFRSocialTrackingProtection(notification) {
  Assert.ok(notification.hidden === false, "Panel should be visible");
  Assert.ok(
    notification.getAttribute("data-notification-category") ===
      "icon_and_message",
    "Panel have corret data attribute"
  );
  Assert.ok(
    notification.querySelector("#cfr-notification-footer-learn-more-link"),
    "Panel should have learn more link"
  );
}

function checkCFRTrackingProtectionMilestone(notification) {
  Assert.ok(notification.hidden === false, "Panel should be visible");
  Assert.ok(
    notification.getAttribute("data-notification-category") === "short_message",
    "Panel have correct data attribute"
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
    action = { type: "FOO" },
    heading_text,
    category = "cfrAddons",
    layout,
    skip_address_bar_notifier = false,
    use_single_secondary_button = false,
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
  if (recommendation.template === "milestone_message") {
    return CFRPageActions.showMilestone(
      browser,
      recommendation,
      // Use the real AS dispatch method to trigger real notifications
      ASRouter.dispatch
    );
  }
  return CFRPageActions.addRecommendation(
    browser,
    trigger,
    recommendation,
    // Use the real AS dispatch method to trigger real notifications
    ASRouter.dispatch
  );
}

add_task(async function setup() {
  // Store it in order to restore to the original value
  const { _fetchLatestAddonVersion } = CFRPageActions;
  // Prevent fetching the real addon url and making a network request
  CFRPageActions._fetchLatestAddonVersion = x => "http://example.com";

  registerCleanupFunction(() => {
    CFRPageActions._fetchLatestAddonVersion = _fetchLatestAddonVersion;
    clearNotifications();
    CFRPageActions.clearRecommendations();
  });
});

add_task(async function test_cfr_notification_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
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
});

add_task(async function test_cfr_notification_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
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
});

add_task(async function test_cfr_notification_minimize() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
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
});

add_task(async function test_cfr_notification_minimize_2() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
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
});

add_task(async function test_cfr_addon_install() {
  // addRecommendation checks that scheme starts with http and host matches
  const browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
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
});

add_task(async function test_cfr_pin_tab_notification_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  const response = await trigger_cfr_panel(browser, "example.com", {
    action: { type: "PIN_CURRENT_TAB" },
    category: "cfrFeatures",
    layout: "message_and_animation",
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

  const notification = document.getElementById(
    "contextual-feature-recommendation-notification"
  );
  checkCFRFeaturesElements(notification);

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

  await BrowserTestUtils.waitForCondition(
    () => gBrowser.selectedTab.pinned,
    "Primary action should pin tab"
  );
  Assert.ok(gBrowser.selectedTab.pinned, "Current tab should be pinned");
  gBrowser.unpinTab(gBrowser.selectedTab);

  // Clicking the primary action also removes the notification
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );
});

add_task(
  async function test_cfr_social_tracking_protection_notification_show() {
    // addRecommendation checks that scheme starts with http and host matches
    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.loadURI(browser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

    const showPanel = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );

    const response = await trigger_cfr_panel(browser, "example.com", {
      action: { type: "OPEN_PROTECTION_PANEL" },
      category: "cfrFeatures",
      layout: "icon_and_message",
      skip_address_bar_notifier: true,
      use_single_secondary_button: true,
    });
    Assert.ok(
      response,
      "Should return true if addRecommendation checks were successful"
    );
    await showPanel;

    const notification = document.getElementById(
      "contextual-feature-recommendation-notification"
    );
    checkCFRSocialTrackingProtection(notification);

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
  }
);

add_task(
  async function test_cfr_tracking_protection_milestone_notification_show() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.contentblocking.cfr-milestone.milestone-achieved", 1000],
        [
          "browser.newtabpage.activity-stream.asrouter.providers.cfr",
          `{"id":"cfr","enabled":true,"type":"local","localProvider":"CFRMessageProvider","frequency":{"custom":[{"period":"daily","cap":10}]},"categories":["cfrAddons","cfrFeatures"],"updateCycleInMs":3600000}`,
        ],
      ],
    });

    // addRecommendation checks that scheme starts with http and host matches
    let browser = gBrowser.selectedBrowser;
    await BrowserTestUtils.loadURI(browser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

    const showPanel = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );

    const response = await trigger_cfr_panel(browser, "example.com", {
      action: { type: "OPEN_PROTECTION_REPORT" },
      category: "cfrFeatures",
      layout: "short_message",
      skip_address_bar_notifier: true,
      heading_text: "Test Milestone Message",
      template: "milestone_message",
    });
    Assert.ok(
      response,
      "Should return true if addRecommendation checks were successful"
    );
    await showPanel;

    const notification = document.getElementById(
      "contextual-feature-recommendation-notification"
    );
    // checkCFRSocialTrackingProtection(notification);
    checkCFRTrackingProtectionMilestone(notification);

    // Check there is a primary button and click it. It will trigger the callback.
    Assert.ok(notification.button);
    let hidePanel = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popuphidden"
    );
    document
      .getElementById("contextual-feature-recommendation-notification")
      .button.click();
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await hidePanel;
  }
);

add_task(async function test_cfr_features_and_addon_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  // Trigger Feature CFR
  let response = await trigger_cfr_panel(browser, "example.com", {
    action: { type: "PIN_CURRENT_TAB" },
    category: "cfrFeatures",
    layout: "message_and_animation",
  });
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
  checkCFRFeaturesElements(notification);

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
  response = await trigger_cfr_panel(browser, "example.com");
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
});

add_task(async function test_cfr_addon_and_features_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
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

  await BrowserTestUtils.loadURI(browser, "about:blank");
  await BrowserTestUtils.browserLoaded(browser, false, "about:blank");

  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  Assert.equal(count, 1, "Count navigation to example.com");

  // Anchor scroll triggers a location change event with the same document
  // https://searchfox.org/mozilla-central/rev/8848b9741fc4ee4e9bc3ae83ea0fc048da39979f/uriloader/base/nsIWebProgressListener.idl#400-403
  await BrowserTestUtils.loadURI(browser, "http://example.com/#foo");
  await BrowserTestUtils.waitForLocationChange(
    gBrowser,
    "http://example.com/#foo"
  );

  Assert.equal(count, 1, "It should ignore same page navigation");

  await BrowserTestUtils.loadURI(browser, TEST_URL);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL);

  Assert.equal(count, 2, "We moved to a new document");

  registerCleanupFunction(() => {
    ASRouterTriggerListeners.get("openURL").uninit();
  });
});

add_task(async function test_matchPattern() {
  let count = 0;
  const triggerHandler = () => ++count;
  const frequentVisitsTrigger = ASRouterTriggerListeners.get("frequentVisits");
  await frequentVisitsTrigger.init(triggerHandler, [], ["*://*.example.com/"]);

  const browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "Registered pattern matched the current location"
  );

  await BrowserTestUtils.loadURI(browser, "about:config");
  await BrowserTestUtils.browserLoaded(browser, false, "about:config");

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "Navigated to a new page but not a match"
  );

  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  await BrowserTestUtils.waitForCondition(
    () => frequentVisitsTrigger._visits.get("example.com").length === 1,
    "Navigated to a location that matches the pattern but within 15 mins"
  );

  await BrowserTestUtils.loadURI(browser, "http://www.example.com/");
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

  registerCleanupFunction(() => {
    ASRouterTriggerListeners.get("frequentVisits").uninit();
  });
});

add_task(async function test_providerNames() {
  const providersBranch =
    "browser.newtabpage.activity-stream.asrouter.providers.";
  const cfrProviderPrefs = Services.prefs.getChildList(providersBranch);
  for (const prefName of cfrProviderPrefs) {
    const prefValue = JSON.parse(Services.prefs.getStringPref(prefName));
    if (prefValue.id) {
      // Snippets are disabled in tests and value is set to []
      Assert.equal(
        prefValue.id,
        prefName.slice(providersBranch.length),
        "Provider id and pref name do not match"
      );
    }
  }
});

add_task(async function test_cfr_notification_keyboard() {
  // addRecommendation checks that scheme starts with http and host matches
  const browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  const response = await trigger_cfr_panel(browser, "example.com");
  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  // Open the panel with the keyboard.
  // Toolbar buttons aren't always focusable; toolbar keyboard navigation
  // makes them focusable on demand. Therefore, we must force focus.
  const button = document.getElementById("contextual-feature-recommendation");
  button.setAttribute("tabindex", "-1");
  button.focus();
  button.removeAttribute("tabindex");

  let focused = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "focus",
    true
  );
  EventUtils.synthesizeKey(" ");
  await focused;
  Assert.ok(true, "Focus inside panel after button pressed");

  let hidden = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  EventUtils.synthesizeKey("KEY_Escape");
  await hidden;
  Assert.ok(true, "Panel hidden after Escape pressed");

  const showPanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // Need to dismiss the notification to clear the RecommendationMap
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  const hidePanel = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popuphidden"
  );
  document
    .getElementById("contextual-feature-recommendation-notification")
    .button.click();
  await hidePanel;
});

add_task(function test_updateCycleForProviders() {
  Services.prefs
    .getChildList("browser.newtabpage.activity-stream.asrouter.providers.")
    .forEach(provider => {
      const prefValue = JSON.parse(Services.prefs.getStringPref(provider, ""));
      if (prefValue.type === "remote-settings") {
        Assert.ok(prefValue.updateCycleInMs);
      }
    });
});

add_task(async function test_heartbeat_tactic_2() {
  clearNotifications();
  registerCleanupFunction(() => {
    // Remove the tab opened by clicking the heartbeat message
    gBrowser.removeCurrentTab();
    clearNotifications();
  });

  const msg = CFRMessageProvider.getMessages().find(
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
    ASRouter.dispatch
  );

  Assert.ok(shown, "Heartbeat CFR added");

  // Wait for visibility change
  BrowserTestUtils.waitForCondition(
    () => document.getElementById("contextual-feature-recommendation"),
    "Heartbeat button exists"
  );

  document.getElementById("contextual-feature-recommendation").click();

  // This will fail if the URL from the message does not load
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    Services.urlFormatter.formatURL(msg.content.action.url)
  );
});
