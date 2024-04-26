/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable @microsoft/sdl/no-insecure-url */
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

add_task(
  async function test_cfr_tracking_protection_milestone_notification_remove() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.contentblocking.cfr-milestone.milestone-achieved", 1000],
        [
          "browser.newtabpage.activity-stream.asrouter.providers.cfr",
          `{"id":"cfr","enabled":true,"type":"local","localProvider":"CFRMessageProvider","updateCycleInMs":3600000}`,
        ],
      ],
    });

    // addRecommendation checks that scheme starts with http and host matches
    let browser = gBrowser.selectedBrowser;
    BrowserTestUtils.startLoadingURIString(browser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");

    const showPanel = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );

    Services.obs.notifyObservers(
      {
        wrappedJSObject: {
          event: "ContentBlockingMilestone",
        },
      },
      "SiteProtection:ContentBlockingMilestone"
    );

    await showPanel;

    const notification = document.getElementById(
      "contextual-feature-recommendation-notification"
    );

    checkCFRTrackingProtectionMilestone(notification);

    Assert.ok(notification.secondaryButton);
    let hidePanel = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popuphidden"
    );

    notification.secondaryButton.click();
    await hidePanel;
    await SpecialPowers.popPrefEnv();
    clearNotifications();
    Services.fog.testResetFOG();
  }
);
