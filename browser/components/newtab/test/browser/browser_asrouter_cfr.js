const { CFRPageActions } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRPageActions.jsm"
);
const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);

const createDummyRecommendation = ({
  action,
  category,
  heading_text,
  layout,
}) => ({
  content: {
    layout: layout || "addon_recommendation",
    category,
    notification_text: "Mochitest",
    heading_text: heading_text || "Mochitest",
    info_icon: {
      label: { attributes: { tooltiptext: "Why am I seeing this" } },
      sumo_path: "extensionrecommendations",
    },
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
});

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
  } = {}
) {
  // a fake action type will result in the action being ignored
  const recommendation = createDummyRecommendation({
    action,
    category,
    heading_text,
    layout,
  });
  if (category !== "cfrAddons") {
    delete recommendation.content.addon;
  }

  clearNotifications();

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

  // This removes the `Addon install failure` notifications
  while (PopupNotifications._currentNotifications.length) {
    PopupNotifications.remove(PopupNotifications._currentNotifications[0]);
  }
  // There should be no more notifications left
  Assert.equal(
    PopupNotifications._currentNotifications.length,
    0,
    "Should have removed the notification"
  );
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
