"use strict";

var notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";
var useOldPrefs = Services.prefs.getBoolPref("browser.preferences.useOldOrganization");
var expectedURL = useOldPrefs ? "about:preferences#content"
                              : "about:preferences#privacy";


add_task(async function test_settingsOpen_observer() {
  info("Opening a dummy tab so openPreferences=>switchToTabHavingURI doesn't use the blank tab.");
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:robots"
  }, async function dummyTabTask(aBrowser) {
    // Ensure preferences is loaded before removing the tab. In the "new prefs", all categories
    // get initialized on page load since we need to be able to search them. Sync is *very*
    // slow to load and therefore we need to wait for it to load when testing the "new prefs".
    // For "old prefs" we only load the actual visited categories so we don't have this problem,
    // as well, the "sync-pane-loaded" notification is not sent on "old prefs".
    let syncPaneLoadedPromise = useOldPrefs || TestUtils.topicObserved("sync-pane-loaded", () => true);

    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, expectedURL);
    info("simulate a notifications-open-settings notification");
    let uri = NetUtil.newURI("https://example.com");
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    Services.obs.notifyObservers(principal, "notifications-open-settings");
    let tab = await tabPromise;
    ok(tab, "The notification settings tab opened");
    await syncPaneLoadedPromise;
    await BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function test_settingsOpen_button() {
  let pm = Services.perms;
  info("Adding notification permission");
  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  try {
    await BrowserTestUtils.withNewTab({
      gBrowser,
      url: notificationURL
    }, async function tabTask(aBrowser) {
      // Ensure preferences is loaded before removing the tab. In the "new prefs", all categories
      // get initialized on page load since we need to be able to search them. Sync is *very*
      // slow to load and therefore we need to wait for it to load when testing the "new prefs".
      // For "old prefs" we only load the actual visited categories so we don't have this problem,
      // as well, the "sync-pane-loaded" notification is not sent on "old prefs".
      let syncPaneLoadedPromise = useOldPrefs || TestUtils.topicObserved("sync-pane-loaded", () => true);

      info("Waiting for notification");
      await openNotification(aBrowser, "showNotification2");

      let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
      if (!alertWindow) {
        ok(true, "Notifications don't use XUL windows on all platforms.");
        await closeNotification(aBrowser);
        return;
      }

      let closePromise = promiseWindowClosed(alertWindow);
      let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, expectedURL);
      let openSettingsMenuItem = alertWindow.document.getElementById("openSettingsMenuItem");
      openSettingsMenuItem.click();

      info("Waiting for notification settings tab");
      let tab = await tabPromise;
      ok(tab, "The notification settings tab opened");

      await syncPaneLoadedPromise;
      await closePromise;
      await BrowserTestUtils.removeTab(tab);
    });
  } finally {
    info("Removing notification permission");
    pm.remove(makeURI(notificationURL), "desktop-notification");
  }
});
