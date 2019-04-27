"use strict";

ChromeUtils.defineModuleGetter(this, "RemoteSettings",
                               "resource://services-settings/remote-settings.js");

const kNotificationId = "fxmonitor";
const kRemoteSettingsKey = "fxmonitor-breaches";

async function fxmonitorNotificationShown() {
  await TestUtils.waitForCondition(() => {
    return PopupNotifications.getNotification(kNotificationId);
  });
  ok(true, "Firefox Monitor PopupNotification was added.");
}

add_task(async function test_remotesettings_get() {
  // Pre-populate the Remote Settings collection with a breach.
  let collection = await RemoteSettings(kRemoteSettingsKey).openCollection();
  let BreachDate = new Date();
  let AddedDate = new Date();
  await collection.create({
    Domain: "example.com",
    Name: "Example Site",
    BreachDate: `${BreachDate.getFullYear()}-${BreachDate.getMonth()}-${BreachDate.getDate()}`,
    AddedDate: `${AddedDate.getFullYear()}-${AddedDate.getMonth()}-${AddedDate.getDate()}`,
    PwnCount: 1000000,
  });
  await collection.db.saveLastModified(1234567);

  // Enable the extension.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.fxmonitor.enabled", true]],
  });

  // Open a tab and wait for the alert.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  await fxmonitorNotificationShown();

  // Clean up.
  BrowserTestUtils.removeTab(tab);
  await collection.clear();
  await SpecialPowers.pushPrefEnv({
    clear: [["extensions.fxmonitor.enabled"],
            ["extensions.fxmonitor.warnedHosts"],
            ["extensions.fxmonitor.firstAlertShown"]],
  });
});
