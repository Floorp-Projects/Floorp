const {CFRPageActions} =
  ChromeUtils.import("resource://activity-stream/lib/CFRPageActions.jsm", {});
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
              data: {url: action.url},
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
  const {_maybeAddAddonInstallURL} = CFRPageActions;
  // Prevent fetching the real addon url and making a network request
  CFRPageActions._maybeAddAddonInstallURL = x => x;

  registerCleanupFunction(() => {
    CFRPageActions._maybeAddAddonInstallURL = _maybeAddAddonInstallURL;
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

  const response = await trigger_cfr_panel(browser, "example.com", {type: "INSTALL_ADDON_FROM_URL", url: "http://example.com"});
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
