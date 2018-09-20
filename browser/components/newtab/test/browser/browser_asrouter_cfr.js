const {CFRPageActions} =
  ChromeUtils.import("resource://activity-stream/lib/CFRPageActions.jsm", {});

function trigger_cfr_panel(browser, trigger, cb) {
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
          },
          secondary: {
            label: {
              value: "Cancel",
              attributes: {accesskey: "C"},
            },
          },
        },
      },
    },
    cb
  );
}

add_task(async function test_cfr_notification_show() {
  // addRecommendation checks that scheme starts with http and host matches
  let browser = gBrowser.selectedBrowser;
  browser.loadURI("http://example.com");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  const response = await trigger_cfr_panel(browser, "example.com", () => {});
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
