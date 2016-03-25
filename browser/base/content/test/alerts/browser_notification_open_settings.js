"use strict";

var notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";

add_task(function* test_settingsOpen_observer() {
  info("Opening a dummy tab so openPreferences=>switchToTabHavingURI doesn't use the blank tab.");
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "about:robots"
  }, function* dummyTabTask(aBrowser) {
    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#content");
    info("simulate a notifications-open-settings notification");
    let uri = NetUtil.newURI("https://example.com");
    let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
    Services.obs.notifyObservers(principal, "notifications-open-settings", null);
    let tab = yield tabPromise;
    ok(tab, "The notification settings tab opened");
    yield BrowserTestUtils.removeTab(tab);
  });
});

add_task(function* test_settingsOpen_button() {
  let pm = Services.perms;
  info("Adding notification permission");
  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  try {
    yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: notificationURL
    }, function* tabTask(aBrowser) {
      info("Waiting for notification");
      yield openNotification(aBrowser, "showNotification2");

      let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
      if (!alertWindow) {
        ok(true, "Notifications don't use XUL windows on all platforms.");
        yield closeNotification(aBrowser);
        return;
      }

      let closePromise = promiseWindowClosed(alertWindow);
      let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:preferences#content");
      let openSettingsMenuItem = alertWindow.document.getElementById("openSettingsMenuItem");
      openSettingsMenuItem.click();

      info("Waiting for notification settings tab");
      let tab = yield tabPromise;
      ok(tab, "The notification settings tab opened");

      yield closePromise;
      yield BrowserTestUtils.removeTab(tab);
    });
  } finally {
    info("Removing notification permission");
    pm.remove(makeURI(notificationURL), "desktop-notification");
  }
});
