const {CFRPageActions} =
  ChromeUtils.import("resource://activity-stream/lib/CFRPageActions.jsm", {});
const {ASRouterTriggerListeners} =
  ChromeUtils.import("resource://activity-stream/lib/ASRouterTriggerListeners.jsm", {});
const {ASRouter} =
  ChromeUtils.import("resource://activity-stream/lib/ASRouter.jsm", {});

function trigger_cfr_panel(browser, trigger, action = {type: "FOO"}) { // a fake action type will result in the action being ignored
  return CFRPageActions.addRecommendation(
    browser,
    trigger,
    {
      content: {
        notification_text: "Mochitest",
        heading_text: "Mochitest",
        info_icon: {
          label: {attributes: {tooltiptext: "Why am I seeing this"}},
          sumo_path: "extensionrecommendations",
        },
        addon: {
          id: "addon-id",
          title: "Addon name",
          icon: "foo",
          author: "Author name",
          amo_url: "https://example.com",
        },
        text: "Mochitest",
        buttons: {
          primary: {
            label: {
              value: "OK",
              attributes: {accesskey: "O"},
            },
            action: {
              type: action.type,
              data: {},
            },
          },
          secondary: [{
            label: {
              value: "Cancel",
              attributes: {accesskey: "C"},
            },
          }, {
            label: {
              value: "Cancel 1",
              attributes: {accesskey: "A"},
            },
          }, {
            label: {
              value: "Cancel 2",
              attributes: {accesskey: "B"},
            },
          }],
        },
      },
    },
    // Use the real AS dispatch method to trigger real notifications
    ASRouter.dispatch
  );
}

add_task(async function setup() {
  // Store it in order to restore to the original value
  const {_fetchLatestAddonVersion} = CFRPageActions;
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
  Assert.ok(response, "Should return true if addRecommendation checks were successful");

  const showPanel = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  Assert.ok(document.getElementById("contextual-feature-recommendation-notification").hidden === false,
    "Panel should be visible");

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(document.getElementById("contextual-feature-recommendation-notification").button);
  let hidePanel = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  document.getElementById("contextual-feature-recommendation-notification").button.click();
  await hidePanel;

  // Clicking the primary action also removes the notification
  Assert.equal(PopupNotifications._currentNotifications.length, 0,
    "Should have removed the notification");
});

add_task(async function test_cfr_addon_install() {
  // addRecommendation checks that scheme starts with http and host matches
  const browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  const response = await trigger_cfr_panel(browser, "example.com", {type: "INSTALL_ADDON_FROM_URL"});
  Assert.ok(response, "Should return true if addRecommendation checks were successful");

  const showPanel = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
  // Open the panel
  document.getElementById("contextual-feature-recommendation").click();
  await showPanel;

  Assert.ok(document.getElementById("contextual-feature-recommendation-notification").hidden === false,
    "Panel should be visible");

  // Check there is a primary button and click it. It will trigger the callback.
  Assert.ok(document.getElementById("contextual-feature-recommendation-notification").button);
  const hidePanel = BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  document.getElementById("contextual-feature-recommendation-notification").button.click();
  await hidePanel;

  await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");

  let [notification] = PopupNotifications.panel.childNodes;
  // Trying to install the addon will trigger a progress popup or an error popup if
  // running the test multiple times in a row
  Assert.ok(notification.id === "addon-progress-notification" ||
    notification.id === "addon-install-failed-notification", "Should try to install the addon");
});

add_task(async function test_onLocationChange_cb() {
  let count = 0;
  const triggerHandler = () => ++count;
  const TEST_URL = "https://example.com/browser/browser/components/newtab/test/browser/blue_page.html";

  ASRouterTriggerListeners.get("openURL").init(triggerHandler, ["example.com"]);

  const browser = gBrowser.selectedBrowser;
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  Assert.equal(count, 1, "Count navigation to example.com");

  // Anchor scroll triggers a location change event with the same document
  // https://searchfox.org/mozilla-central/rev/8848b9741fc4ee4e9bc3ae83ea0fc048da39979f/uriloader/base/nsIWebProgressListener.idl#400-403
  await BrowserTestUtils.loadURI(browser, "http://example.com/#foo");
  await BrowserTestUtils.waitForLocationChange(gBrowser, "http://example.com/#foo");

  Assert.equal(count, 1, "It should ignore same page navigation");

  await BrowserTestUtils.loadURI(browser, TEST_URL);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL);

  Assert.equal(count, 2, "We moved to a new document");
});
