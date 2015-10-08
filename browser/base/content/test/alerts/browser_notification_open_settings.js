"use strict";

add_task(function* test_settingsOpen() {
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
