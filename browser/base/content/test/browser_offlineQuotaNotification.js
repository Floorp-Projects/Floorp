/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test offline quota warnings - must be run as a mochitest-browser test or
// else the test runner gets in the way of notifications due to bug 857897.

const URL = "http://mochi.test:8888/browser/browser/base/content/test/offlineQuotaNotification.html";

registerCleanupFunction(function() {
  // Clean up after ourself
  let uri = Services.io.newURI(URL, null, null);
  var principal = Services.scriptSecurityManager.getNoAppCodebasePrincipal(uri);
  Services.perms.removeFromPrincipal(principal, "offline-app");
  Services.prefs.clearUserPref("offline-apps.quota.warn");
  Services.prefs.clearUserPref("offline-apps.allow_by_default");
});

// Check that the "preferences" UI is opened and showing which websites have
// offline storage permissions - currently this is the "network" tab in the
// "advanced" pane.
function checkPreferences(prefsWin) {
  // We expect a 'paneload' event for the 'advanced' pane, then
  // a 'select' event on the 'network' tab inside that pane.
  prefsWin.addEventListener("paneload", function paneload(evt) {
    prefsWin.removeEventListener("paneload", paneload);
    is(evt.target.id, "paneAdvanced", "advanced pane loaded");
    let tabPanels = evt.target.getElementsByTagName("tabpanels")[0];
    tabPanels.addEventListener("select", function tabselect() {
      tabPanels.removeEventListener("select", tabselect);
      is(tabPanels.selectedPanel.id, "networkPanel", "networkPanel is selected");
      // all good, we are done.
      prefsWin.close();
      finish();
    });
  });
}

function test() {
  waitForExplicitFinish();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    gBrowser.selectedBrowser.contentWindow.applicationCache.oncached = function() {
      executeSoon(function() {
        // We got cached - now we should have provoked the quota warning.
        let notification = PopupNotifications.getNotification('offline-app-usage');
        ok(notification, "have offline-app-usage notification");
        // select the default action - this should cause the preferences
        // window to open - which we track either via a window watcher (for
        // the window-based prefs) or via an "Initialized" event (for
        // in-content prefs.)
        if (Services.prefs.getBoolPref("browser.preferences.inContent")) {
          // Bug 881576 - ensure this works with inContent prefs.
          todo(false, "Bug 881576 - this test needs to be updated for inContent prefs");
        } else {
          Services.ww.registerNotification(function wwobserver(aSubject, aTopic, aData) {
            if (aTopic != "domwindowopened")
              return;
            Services.ww.unregisterNotification(wwobserver);
            checkPreferences(aSubject);
          });
          PopupNotifications.panel.firstElementChild.button.click();
        }
      });
    };
    Services.prefs.setIntPref("offline-apps.quota.warn", 1);

    // Click the notification panel's "Allow" button.  This should kick
    // off updates which will call our oncached handler above.
    PopupNotifications.panel.firstElementChild.button.click();
  }, true);

  Services.prefs.setBoolPref("offline-apps.allow_by_default", false);
  gBrowser.contentWindow.location = URL;
}
