/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test offline quota warnings - must be run as a mochitest-browser test or
// else the test runner gets in the way of notifications due to bug 857897.

const URL = "http://mochi.test:8888/browser/browser/base/content/test/general/offlineQuotaNotification.html";

registerCleanupFunction(function() {
  // Clean up after ourself
  let uri = Services.io.newURI(URL, null, null);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});
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
// Same as the other one, but for in-content preferences
function checkInContentPreferences(win) {
  let doc = win.document;
  let sel = doc.getElementById("categories").selectedItems[0].id;
  let tab = doc.getElementById("advancedPrefs").selectedTab.id;
  is(gBrowser.currentURI.spec, "about:preferences#advanced", "about:preferences loaded");
  is(sel, "category-advanced", "Advanced pane was selected");
  is(tab, "networkTab", "Network tab is selected");
  // all good, we are done.
  win.close();
  finish();
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("offline-apps.allow_by_default", false);

  // Open a new tab.
  gBrowser.selectedTab = gBrowser.addTab(URL);
  registerCleanupFunction(() => gBrowser.removeCurrentTab());


  Promise.all([
    // Wait for a notification that asks whether to allow offline storage.
    promiseNotification(),
    // Wait for the tab to load.
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser)
  ]).then(() => {
    gBrowser.selectedBrowser.contentWindow.applicationCache.oncached = function() {
      executeSoon(function() {
        // We got cached - now we should have provoked the quota warning.
        let notification = PopupNotifications.getNotification('offline-app-usage');
        ok(notification, "have offline-app-usage notification");
        // select the default action - this should cause the preferences
        // window to open - which we track either via a window watcher (for
        // the window-based prefs) or via an "Initialized" event (for
        // in-content prefs.)
        if (!Services.prefs.getBoolPref("browser.preferences.inContent")) {
          Services.ww.registerNotification(function wwobserver(aSubject, aTopic, aData) {
            if (aTopic != "domwindowopened")
              return;
            Services.ww.unregisterNotification(wwobserver);
            checkPreferences(aSubject);
          });
        }
        PopupNotifications.panel.firstElementChild.button.click();
        if (Services.prefs.getBoolPref("browser.preferences.inContent")) {
          let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
          newTabBrowser.addEventListener("Initialized", function PrefInit() {
            newTabBrowser.removeEventListener("Initialized", PrefInit, true);
            executeSoon(function() {
              checkInContentPreferences(newTabBrowser.contentWindow);
            })
          }, true);
        }
      });
    };
    Services.prefs.setIntPref("offline-apps.quota.warn", 1);

    // Click the notification panel's "Allow" button.  This should kick
    // off updates which will call our oncached handler above.
    PopupNotifications.panel.firstElementChild.button.click();
  });
}

function promiseNotification() {
  return new Promise(resolve => {
    PopupNotifications.panel.addEventListener("popupshown", function onShown() {
      PopupNotifications.panel.removeEventListener("popupshown", onShown);
      resolve();
    });
  });
}
