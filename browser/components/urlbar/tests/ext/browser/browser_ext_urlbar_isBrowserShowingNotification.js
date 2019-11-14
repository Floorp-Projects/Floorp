/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.isBrowserShowingNotification
// WebExtension Experiment API.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
});

// isBrowserShowingNotification should return false when there are no
// notifications.
add_task(async function noNotifications() {
  // First, hide notifications that are currently showing.

  // Escape closes many doorhangers and panels: tracking protection, site
  // identity, page action panels, toolbar button panels.
  EventUtils.synthesizeKey("KEY_Escape");

  // urlbar view
  await UrlbarTestUtils.promisePopupClose(window);

  // notification box (info bar)
  window.gBrowser.getNotificationBox().removeAllNotifications(true);

  // app menu notification doorhanger
  if (AppMenuNotifications.activeNotification) {
    AppMenuNotifications.dismissNotification(
      AppMenuNotifications.activeNotification.id
    );
  }

  await checkExtension(false);
});

// isBrowserShowingNotification should return true when the urlbar view is open.
add_task(async function urlbarView() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
    waitForFocus,
  });
  await checkExtension(true);
  await UrlbarTestUtils.promisePopupClose(window);
  await checkExtension(false);
});

// isBrowserShowingNotification should return true when the tracking protection
// doorhanger is showing.
add_task(async function trackingProtection() {
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    let panel = document.getElementById("protections-popup");
    document.getElementById("tracking-protection-icon-container").click();
    await BrowserTestUtils.waitForPopupEvent(panel, "shown");

    await checkExtension(true);

    EventUtils.synthesizeKey("KEY_Escape");
    await BrowserTestUtils.waitForPopupEvent(panel, "hidden");

    await checkExtension(false);
  });
});

// isBrowserShowingNotification should return true when the site identity
// doorhanger is showing.
add_task(async function siteIdentity() {
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    let panel = document.getElementById("identity-popup");
    document.getElementById("identity-box").click();
    await BrowserTestUtils.waitForPopupEvent(panel, "shown");

    await checkExtension(true);

    EventUtils.synthesizeKey("KEY_Escape");
    await BrowserTestUtils.waitForPopupEvent(panel, "hidden");

    await checkExtension(false);
  });
});

// isBrowserShowingNotification should return true when a notification box (info
// bar) is showing.
add_task(async function notificationBox() {
  let box = gBrowser.getNotificationBox();
  let note = box.appendNotification(
    "Test",
    "urlbar-test",
    null,
    box.PRIORITY_INFO_HIGH,
    null,
    null,
    null
  );
  await checkExtension(true);
  box.removeNotification(note, true);
  await checkExtension(false);
});

// isBrowserShowingNotification should return true when a page action panel is
// showing.
add_task(async function pageActionPanel() {
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    let panel = document.getElementById("pageActionPanel");
    document.getElementById("pageActionButton").click();
    await BrowserTestUtils.waitForPopupEvent(panel, "shown");

    await checkExtension(true);

    EventUtils.synthesizeKey("KEY_Escape");
    await BrowserTestUtils.waitForPopupEvent(panel, "hidden");

    await checkExtension(false);
  });
});

// isBrowserShowingNotification should return true when a toolbar button panel
// is showing.
add_task(async function toolbarButtonPanel() {
  document.getElementById("library-button").click();

  // CustomizableUI widget panels are temporary, so just wait until the panel
  // exists.
  await TestUtils.waitForCondition(() => {
    return document.getElementById("customizationui-widget-panel");
  });

  await checkExtension(true);

  EventUtils.synthesizeKey("KEY_Escape");
  await TestUtils.waitForCondition(() => {
    return !document.getElementById("customizationui-widget-panel");
  });

  await checkExtension(false);
});

// isBrowserShowingNotification should return true when an app menu notification
// is showing.
add_task(async function appMenuNotification() {
  AppMenuNotifications.showNotification("update-manual", {
    callback: () => {},
  });
  let panel = document.getElementById("appMenu-notification-popup");
  await BrowserTestUtils.waitForPopupEvent(panel, "shown");

  await checkExtension(true);

  AppMenuNotifications.dismissNotification("update-manual");
  await BrowserTestUtils.waitForPopupEvent(panel, "hidden");

  await checkExtension(false);

  // The notification remains in AppMenuNotifications and the badge remains on
  // the button even though the panel was dismissed, so remove it for good now
  // so it doesn't interfere with other tests.
  AppMenuNotifications.removeNotification("update-manual");
});

async function checkExtension(expectedShowing) {
  let ext = await loadExtension(async () => {
    let showing = await browser.experiments.urlbar.isBrowserShowingNotification();
    browser.test.sendMessage("showing", showing);
  });
  let showing = await ext.awaitMessage("showing");
  Assert.equal(showing, expectedShowing);
  await ext.unload();
}
