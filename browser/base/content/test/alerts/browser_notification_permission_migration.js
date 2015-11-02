const UI_VERSION = 32;

var gBrowserGlue = Cc["@mozilla.org/browser/browserglue;1"]
                     .getService(Ci.nsIObserver);
var notificationURI = makeURI("http://example.org");
var pm = Services.perms;
var currentUIVersion;

add_task(function* setup() {
  currentUIVersion = Services.prefs.getIntPref("browser.migration.version");
  Services.prefs.setIntPref("browser.migration.version", UI_VERSION - 1);
  pm.add(notificationURI, "desktop-notification", pm.ALLOW_ACTION);
});

add_task(function* test_permissionMigration() {
  if ("@mozilla.org/system-alerts-service;1" in Cc) {
    ok(true, "Notifications don't use XUL windows on all platforms.");
    return;
  }

  info("Waiting for migration notification");
  let alertWindowPromise = promiseAlertWindow();
  gBrowserGlue.observe(null, "browser-glue-test", "force-ui-migration");
  let alertWindow = yield alertWindowPromise;

  info("Clicking on notification");
  let url = Services.urlFormatter.formatURLPref("browser.push.warning.infoURL");
  let closePromise = promiseWindowClosed(alertWindow);
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url);
  EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"), {}, alertWindow);

  info("Waiting for migration info tab");
  let tab = yield tabPromise;
  ok(tab, "The migration info tab opened");

  yield closePromise;
  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* cleanup() {
  Services.prefs.setIntPref("browser.migration.version", currentUIVersion);
  pm.remove(notificationURI, "desktop-notification");
});
