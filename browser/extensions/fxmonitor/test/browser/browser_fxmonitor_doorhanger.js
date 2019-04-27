"use strict";

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "RemoteSettings",
                               "resource://services-settings/remote-settings.js");

const kNotificationId = "fxmonitor";
const kRemoteSettingsKey = "fxmonitor-breaches";

async function fxmonitorNotificationShown() {
  await TestUtils.waitForCondition(() => {
    return PopupNotifications.getNotification(kNotificationId)
      && PopupNotifications.panel.state == "open";
  }, "Waiting for fxmonitor notification to be shown");
  ok(true, "Firefox Monitor PopupNotification was added.");
}

async function fxmonitorNotificationGone() {
  await TestUtils.waitForCondition(() => {
    return !PopupNotifications.getNotification(kNotificationId)
      && PopupNotifications.panel.state == "closed";
  }, "Waiting for fxmonitor notification to go away");
  ok(true, "Firefox Monitor PopupNotification was removed.");
}

let cps2 = Cc["@mozilla.org/content-pref/service;1"]
             .getService(Ci.nsIContentPrefService2);

async function clearWarnedHosts() {
  return new Promise((resolve, reject) => {
    cps2.removeByName("extensions.fxmonitor.hostAlreadyWarned", Cu.createLoadContext(), {
      handleCompletion: resolve,
    });
  });
}

add_task(async function test_warnedHosts_migration() {
  info("Test that we correctly migrate the warnedHosts pref to content prefs.");

  // Pre-set the warnedHosts pref to include example.com.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.fxmonitor.warnedHosts", "[\"example.com\"]"]],
  });

  // Pre-populate the Remote Settings collection with a breach.
  let collection = await RemoteSettings(kRemoteSettingsKey).openCollection();
  let BreachDate = new Date();
  let AddedDate = new Date();
  await collection.create({
    Domain: "example.com",
    Name: "Example Site",
    BreachDate: `${BreachDate.getFullYear()}-${BreachDate.getMonth() + 1}-${BreachDate.getDate()}`,
    AddedDate: `${AddedDate.getFullYear()}-${AddedDate.getMonth() + 1}-${AddedDate.getDate()}`,
    PwnCount: 1000000,
  });
  await collection.db.saveLastModified(1234567);

  // Finally, reload the extension.
  let addon = await AddonManager.getAddonByID("fxmonitor@mozilla.org");
  await addon.reload();

  await TestUtils.waitForCondition(() => {
    return !Services.prefs.prefHasUserValue("extensions.fxmonitor.warnedHosts");
  }, "Waiting for the warnedHosts pref to be cleared");
  ok(true, "The warnedHosts pref was cleared");

  // Open a tab and ensure the alert isn't shown.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  await fxmonitorNotificationGone();

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  await collection.clear();
  await collection.db.saveLastModified(1234567);
  // Trigger a sync to clear.
  await RemoteSettings(kRemoteSettingsKey).emit("sync", {
    data: {
      current: [],
    },
  });
  await clearWarnedHosts();
  await SpecialPowers.pushPrefEnv({
    clear: [["extensions.fxmonitor.enabled"],
            ["extensions.fxmonitor.firstAlertShown"]],
  });
});

add_task(async function test_main_flow() {
  info("Test that we show the first alert correctly for a recent breach.");

  // Pre-populate the Remote Settings collection with a breach.
  let collection = await RemoteSettings(kRemoteSettingsKey).openCollection();
  let BreachDate = new Date();
  let AddedDate = new Date();
  await collection.create({
    Domain: "example.com",
    Name: "Example Site",
    BreachDate: `${BreachDate.getFullYear()}-${BreachDate.getMonth() + 1}-${BreachDate.getDate()}`,
    AddedDate: `${AddedDate.getFullYear()}-${AddedDate.getMonth() + 1}-${AddedDate.getDate()}`,
    PwnCount: 1000000,
  });
  await collection.db.saveLastModified(1234567);

  // Trigger a sync.
  await RemoteSettings(kRemoteSettingsKey).emit("sync", {
    data: {
      current: await RemoteSettings(kRemoteSettingsKey).get(),
    },
  });

  // Enable the extension.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.fxmonitor.FirefoxMonitorURL", "http://example.org"]],
  });

  // Open a tab and wait for the alert.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  await fxmonitorNotificationShown();

  // Test that dismissing works.
  let notification = Array.prototype.find.call(PopupNotifications.panel.children,
    elt => elt.getAttribute("popupid") == kNotificationId);
  EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});
  await fxmonitorNotificationGone();

  // Reload and make sure the alert isn't shown again.
  let promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await fxmonitorNotificationGone();

  // Reset state.
  await collection.clear();
  await collection.db.saveLastModified(1234567);
  await clearWarnedHosts();
  await SpecialPowers.pushPrefEnv({
    clear: [["extensions.fxmonitor.firstAlertShown"]],
  });

  // Reload and wait for the alert.
  promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await fxmonitorNotificationShown();

  // Test that the primary button opens Firefox Monitor in a new tab.
  notification = Array.prototype.find.call(PopupNotifications.panel.children,
    elt => elt.getAttribute("popupid") == kNotificationId);
  let url =
    `http://example.org/?breach=${encodeURIComponent("Example Site")}&utm_source=firefox&utm_medium=popup`;
  promise = BrowserTestUtils.waitForNewTab(gBrowser, url);
  EventUtils.synthesizeMouseAtCenter(notification.button, {});
  let newtab = await promise;

  // Close the new tab and check that the alert is gone.
  BrowserTestUtils.removeTab(newtab);
  await fxmonitorNotificationGone();

  // Reset state (but not firstAlertShown).
  await collection.clear();
  await collection.db.saveLastModified(1234567);
  await clearWarnedHosts();

  info("Test that we do not show the second alert for a breach added over two months ago.");

  // Add a new "old" breach - added over 2 months ago.
  AddedDate.setMonth(AddedDate.getMonth() - 3);
  await collection.create({
    Domain: "example.com",
    Name: "Example Site",
    BreachDate: `${BreachDate.getFullYear()}-${BreachDate.getMonth() + 1}-${BreachDate.getDate()}`,
    AddedDate: `${AddedDate.getFullYear()}-${AddedDate.getMonth() + 1}-${AddedDate.getDate()}`,
    PwnCount: 1000000,
  });
  await collection.db.saveLastModified(1234567);

  // Trigger a sync.
  await RemoteSettings(kRemoteSettingsKey).emit("sync", {
    data: {
      current: await RemoteSettings(kRemoteSettingsKey).get(),
    },
  });

  // Check that there's no alert for the old breach.
  promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await fxmonitorNotificationGone();

  // Reset state (but not firstAlertShown).
  AddedDate.setMonth(AddedDate.getMonth() + 3);
  await collection.clear();
  await collection.db.saveLastModified(1234567);
  await clearWarnedHosts();

  info("Test that we do show the second alert for a recent breach.");

  // Add a new "recent" breach.
  await collection.create({
    Domain: "example.com",
    Name: "Example Site",
    BreachDate: `${BreachDate.getFullYear()}-${BreachDate.getMonth() + 1}-${BreachDate.getDate()}`,
    AddedDate: `${AddedDate.getFullYear()}-${AddedDate.getMonth() + 1}-${AddedDate.getDate()}`,
    PwnCount: 1000000,
  });
  await collection.db.saveLastModified(1234567);

  // Trigger a sync.
  await RemoteSettings(kRemoteSettingsKey).emit("sync", {
    data: {
      current: await RemoteSettings(kRemoteSettingsKey).get(),
    },
  });

  // Check that there's an alert for the new breach.
  promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await fxmonitorNotificationShown();

  // Reset state (including firstAlertShown)
  await collection.clear();
  await collection.db.saveLastModified(1234567);
  await clearWarnedHosts();
  await SpecialPowers.pushPrefEnv({
    clear: [["extensions.fxmonitor.firstAlertShown"]],
  });

  info("Test that we do not show the first alert for a breach added over a year ago.");

  // Add a new "old" breach - added over a year ago.
  AddedDate.setFullYear(AddedDate.getFullYear() - 2);
  await collection.create({
    Domain: "example.com",
    Name: "Example Site",
    BreachDate: `${BreachDate.getFullYear()}-${BreachDate.getMonth() + 1}-${BreachDate.getDate()}`,
    AddedDate: `${AddedDate.getFullYear()}-${AddedDate.getMonth() + 1}-${AddedDate.getDate()}`,
    PwnCount: 1000000,
  });
  await collection.db.saveLastModified(1234567);

  // Trigger a sync.
  await RemoteSettings(kRemoteSettingsKey).emit("sync", {
    data: {
      current: await RemoteSettings(kRemoteSettingsKey).get(),
    },
  });

  // Check that there's no alert for the old breach.
  promise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  await promise;
  await fxmonitorNotificationGone();

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  await collection.clear();
  await collection.db.saveLastModified(1234567);
  // Trigger a sync to clear.
  await RemoteSettings(kRemoteSettingsKey).emit("sync", {
    data: {
      current: [],
    },
  });
  await clearWarnedHosts();
  await SpecialPowers.pushPrefEnv({
    clear: [["extensions.fxmonitor.firstAlertShown"]],
  });
});
