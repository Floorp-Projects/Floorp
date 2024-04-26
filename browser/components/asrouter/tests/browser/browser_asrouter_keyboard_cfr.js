/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable @microsoft/sdl/no-insecure-url */
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

add_task(async function test_cfr_notification_keyboard() {
  // addRecommendation checks that scheme starts with http and host matches
  const browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

  clearNotifications();

  let recommendation = {
    template: "cfr_doorhanger",
    groups: ["mochitest-group"],
    content: {
      layout: "addon_recommendation",
      category: "cfrAddons",
      anchor_id: "page-action-buttons",
      icon_class: "cfr-doorhanger-medium-icon",
      skip_address_bar_notifier: false,
      heading_text: "Sample Mochitest",
      icon: "chrome://activity-stream/content/data/content/assets/glyph-webextension-16.svg",
      icon_dark_theme:
        "chrome://activity-stream/content/data/content/assets/glyph-webextension-16.svg",
      info_icon: {
        label: { attributes: { tooltiptext: "Why am I seeing this" } },
        sumo_path: "extensionrecommendations",
      },
      addon: {
        id: "addon-id",
        title: "Addon name",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        author: "Author name",
        amo_url: "https://example.com",
        rating: "4.5",
        users: "1.1M",
      },
      text: "Mochitest",
      buttons: {
        primary: {
          label: {
            value: "OK",
            attributes: { accesskey: "O" },
          },
          action: {
            type: "CANCEL",
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
        ],
      },
    },
  };

  recommendation.content.notification_text = new String("Mochitest"); // eslint-disable-line
  recommendation.content.notification_text.attributes = {
    tooltiptext: "Mochitest tooltip",
    "a11y-announcement": "Mochitest announcement",
  };

  const response = await CFRPageActions.addRecommendation(
    gBrowser.selectedBrowser,
    "example.com",
    recommendation,
    // Use the real AS dispatch method to trigger real notifications
    ASRouter.dispatchCFRAction
  );

  Assert.ok(
    response,
    "Should return true if addRecommendation checks were successful"
  );

  // Open the panel with the keyboard.
  // Toolbar buttons aren't always focusable; toolbar keyboard navigation
  // makes them focusable on demand. Therefore, we must force focus.
  const button = document.getElementById("contextual-feature-recommendation");
  button.setAttribute("tabindex", "-1");

  let buttonFocused = BrowserTestUtils.waitForEvent(button, "focus");
  button.focus();
  await buttonFocused;

  Assert.ok(true, "Focus page action button");

  let focused = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "focus",
    true
  );

  EventUtils.synthesizeKey(" ");
  await focused;
  Assert.ok(true, "Focus inside panel after button pressed");

  button.removeAttribute("tabindex");

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
  Services.fog.testResetFOG();
});
