/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const TESTROOT2 = "http://example.org/browser/toolkit/mozapps/extensions/test/xpinstall/";
const SECUREROOT = "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const PROGRESS_NOTIFICATION = "addon-progress";

const { REQUIRE_SIGNING } = Cu.import("resource://gre/modules/addons/AddonConstants.jsm", {});
const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

var rootDir = getRootDirectory(gTestPath);
var rootPath = rootDir.split('/');
var chromeName = rootPath[0] + '//' + rootPath[2];
var croot = chromeName + "/content/browser/toolkit/mozapps/extensions/test/xpinstall/";
var jar = getJar(croot);
if (jar) {
  var tmpdir = extractJarToTmp(jar);
  croot = 'file://' + tmpdir.path + '/';
}
const CHROMEROOT = croot;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;

function getObserverTopic(aNotificationId) {
  let topic = aNotificationId;
  if (topic == "xpinstall-disabled")
    topic = "addon-install-disabled";
  else if (topic == "addon-progress")
    topic = "addon-install-started";
  else if (topic == "addon-install-restart")
    topic = "addon-install-complete";
  return topic;
}

function waitForProgressNotification(aPanelOpen = false, aExpectedCount = 1) {
  return Task.spawn(function* () {
    let notificationId = PROGRESS_NOTIFICATION;
    info("Waiting for " + notificationId + " notification");

    let topic = getObserverTopic(notificationId);

    let observerPromise = new Promise(resolve => {
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        // Ignore the progress notification unless that is the notification we want
        if (notificationId != PROGRESS_NOTIFICATION &&
            aTopic == getObserverTopic(PROGRESS_NOTIFICATION)) {
          return;
        }
        Services.obs.removeObserver(observer, topic);
        resolve();
      }, topic, false);
    });

    let panelEventPromise;
    if (aPanelOpen) {
      panelEventPromise = Promise.resolve();
    } else {
      panelEventPromise = new Promise(resolve => {
        PopupNotifications.panel.addEventListener("popupshowing", function eventListener() {
          PopupNotifications.panel.removeEventListener("popupshowing", eventListener);
          resolve();
        });
      });
    }

    yield observerPromise;
    yield panelEventPromise;

    info("Saw a notification");
    ok(PopupNotifications.isPanelOpen, "Panel should be open");
    is(PopupNotifications.panel.childNodes.length, aExpectedCount, "Should be the right number of notifications");
    if (PopupNotifications.panel.childNodes.length) {
      let nodes = Array.from(PopupNotifications.panel.childNodes);
      let notification = nodes.find(n => n.id == notificationId + "-notification");
      ok(notification, `Should have seen the right notification`);
      ok(notification.button.hasAttribute("disabled"),
         "The install button should be disabled");
    }

    return PopupNotifications.panel;
  });
}

function waitForNotification(aId, aExpectedCount = 1) {
  return Task.spawn(function* () {
    info("Waiting for " + aId + " notification");

    let topic = getObserverTopic(aId);

    let observerPromise = new Promise(resolve => {
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        // Ignore the progress notification unless that is the notification we want
        if (aId != PROGRESS_NOTIFICATION &&
            aTopic == getObserverTopic(PROGRESS_NOTIFICATION)) {
          return;
        }
        Services.obs.removeObserver(observer, topic);
        resolve();
      }, topic, false);
    });

    let panelEventPromise = new Promise(resolve => {
      PopupNotifications.panel.addEventListener("PanelUpdated", function eventListener(e) {
        // Skip notifications that are not the one that we are supposed to be looking for
        if (e.detail.indexOf(aId) == -1) {
          return;
        }
        PopupNotifications.panel.removeEventListener("PanelUpdated", eventListener);
        resolve();
      });
    });

    yield observerPromise;
    yield panelEventPromise;

    info("Saw a " + aId + " notification");
    ok(PopupNotifications.isPanelOpen, "Panel should be open");
    is(PopupNotifications.panel.childNodes.length, aExpectedCount, "Should be the right number of notifications");
    if (PopupNotifications.panel.childNodes.length) {
      let nodes = Array.from(PopupNotifications.panel.childNodes);
      let notification = nodes.find(n => n.id == aId + "-notification");
      ok(notification, "Should have seen the " + aId + " notification");
    }

    return PopupNotifications.panel;
  });
}

function waitForNotificationClose() {
  return new Promise(resolve => {
    info("Waiting for notification to close");
    PopupNotifications.panel.addEventListener("popuphidden", function listener() {
      PopupNotifications.panel.removeEventListener("popuphidden", listener, false);
      resolve();
    }, false);
  });
}

function waitForInstallDialog() {
  return Task.spawn(function* () {
    if (Preferences.get("xpinstall.customConfirmationUI", false)) {
      let panel = yield waitForNotification("addon-install-confirmation");
      return panel.childNodes[0];
    }

    info("Waiting for install dialog");

    let window = yield new Promise(resolve => {
      Services.wm.addListener({
        onOpenWindow(aXULWindow) {
          Services.wm.removeListener(this);
          resolve(aXULWindow);
        },
        onCloseWindow(aXULWindow) {
        },
        onWindowTitleChange(aXULWindow, aNewTitle) {
        }
      });
    });
    info("Install dialog opened, waiting for focus");

    let domwindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindow);
    yield new Promise(resolve => {
      waitForFocus(function() {
        resolve();
      }, domwindow);
    });
    info("Saw install dialog");
    is(domwindow.document.location.href, XPINSTALL_URL, "Should have seen the right window open");

    // Override the countdown timer on the accept button
    let button = domwindow.document.documentElement.getButton("accept");
    button.disabled = false;

    return null;
  });
}

function removeTab() {
  return Promise.all([
    waitForNotificationClose(),
    BrowserTestUtils.removeTab(gBrowser.selectedTab)
  ]);
}

function acceptInstallDialog(installDialog) {
  if (Preferences.get("xpinstall.customConfirmationUI", false)) {
    installDialog.button.click();
  } else {
    let win = Services.wm.getMostRecentWindow("Addons:Install");
    win.document.documentElement.acceptDialog();
  }
}

function cancelInstallDialog(installDialog) {
  if (Preferences.get("xpinstall.customConfirmationUI", false)) {
    installDialog.secondaryButton.click();
  } else {
    let win = Services.wm.getMostRecentWindow("Addons:Install");
    win.document.documentElement.cancelDialog();
  }
}

function waitForSingleNotification(aCallback) {
  return Task.spawn(function* () {
    while (PopupNotifications.panel.childNodes.length == 2) {
      yield new Promise(resolve => executeSoon(resolve));

      info("Waiting for single notification");
      // Notification should never close while we wait
      ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    }
  });
}

function setupRedirect(aSettings) {
  var url = "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/redirect.sjs?mode=setup";
  for (var name in aSettings) {
    url += "&" + name + "=" + aSettings[name];
  }

  var req = new XMLHttpRequest();
  req.open("GET", url, false);
  req.send(null);
}

function getInstalls() {
  return new Promise(resolve => {
    AddonManager.getAllInstalls(installs => resolve(installs));
  });
}

var TESTS = [
function test_disabledInstall() {
  return Task.spawn(function* () {
    Services.prefs.setBoolPref("xpinstall.enabled", false);

    let notificationPromise = waitForNotification("xpinstall-disabled");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.button.label, "Enable", "Should have seen the right button");
    is(notification.getAttribute("label"),
       "Software installation is currently disabled. Click Enable and try again.");

    let closePromise = waitForNotificationClose();
    // Click on Enable
    EventUtils.synthesizeMouseAtCenter(notification.button, {});
    yield closePromise;

    try {
      ok(Services.prefs.getBoolPref("xpinstall.enabled"), "Installation should be enabled");
    } catch (e) {
      ok(false, "xpinstall.enabled should be set");
    }

    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
    let installs = yield getInstalls();
    is(installs.length, 0, "Shouldn't be any pending installs");
  });
},

function test_blockedInstall() {
  return Task.spawn(function* () {
    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.button.label, "Allow", "Should have seen the right button");
    is(notification.getAttribute("origin"), "example.com",
       "Should have seen the right origin host");
    is(notification.getAttribute("label"),
       gApp + " prevented this site from asking you to install software on your computer.",
       "Should have seen the right message");

    let dialogPromise = waitForInstallDialog();
    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});
    // Notification should have changed to progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    notification = panel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");
    let installDialog = yield dialogPromise;

    notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    panel = yield notificationPromise;

    notification = panel.childNodes[0];
    is(notification.button.label, "Restart Now", "Should have seen the right button");
    is(notification.getAttribute("label"),
       "XPI Test will be installed after you restart " + gApp + ".",
       "Should have seen the right message");

    let installs = yield getInstalls();
    is(installs.length, 1, "Should be one pending install");
    installs[0].cancel();
    yield removeTab();
  });
},

function test_whitelistedInstall() {
  return Task.spawn(function* () {
    let originalTab = gBrowser.selectedTab;
    let tab;
    gBrowser.selectedTab = originalTab;
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?"
      + triggers).then(newTab => tab = newTab);
    yield progressPromise;
    let installDialog = yield dialogPromise;
    yield BrowserTestUtils.waitForCondition(() => !!tab, "tab should be present");

    is(gBrowser.selectedTab, tab,
       "tab selected in response to the addon-install-confirmation notification");

    let notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.button.label, "Restart Now", "Should have seen the right button");
    is(notification.getAttribute("label"),
             "XPI Test will be installed after you restart " + gApp + ".",
             "Should have seen the right message");

    let installs = yield getInstalls();
    is(installs.length, 1, "Should be one pending install");
    installs[0].cancel();

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_failedDownload() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let failPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "missing.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let panel = yield failPromise;

    let notification = panel.childNodes[0];
    is(notification.getAttribute("label"),
       "The add-on could not be downloaded because of a connection failure.",
       "Should have seen the right message");

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_corruptFile() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let failPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "corrupt.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let panel = yield failPromise;

    let notification = panel.childNodes[0];
    is(notification.getAttribute("label"),
       "The add-on downloaded from this site could not be installed " +
       "because it appears to be corrupt.",
       "Should have seen the right message");

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_incompatible() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let failPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "incompatible.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let panel = yield failPromise;

    let notification = panel.childNodes[0];
    is(notification.getAttribute("label"),
       "XPI Test could not be installed because it is not compatible with " +
       gApp + " " + gVersion + ".",
       "Should have seen the right message");

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_restartless() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "restartless.xpi"
    }));
    gBrowser.selectedTab = gBrowser.addTab();
    gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let installDialog = yield dialogPromise;

    let notificationPromise = waitForNotification("addon-install-complete");
    acceptInstallDialog(installDialog);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.getAttribute("label"),
       "XPI Test has been installed successfully.",
       "Should have seen the right message");

    let installs = yield getInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = yield new Promise(resolve => {
      AddonManager.getAddonByID("restartless-xpi@tests.mozilla.org", result => {
        resolve(result);
      });
    });
    addon.uninstall();

    Services.perms.remove(makeURI("http://example.com/"), "install");

    let closePromise = waitForNotificationClose();
    gBrowser.removeTab(gBrowser.selectedTab);
    yield closePromise;
  });
},

function test_sequential() {
  return Task.spawn(function* () {
    // This test is only relevant if using the new doorhanger UI
    // TODO: this subtest is disabled until multiple notification prompts are
    // reworked in bug 1188152
    if (true || !Preferences.get("xpinstall.customConfirmationUI", false)) {
      return;
    }
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "Restartless XPI": "restartless.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let installDialog = yield dialogPromise;

    // Should see the right add-on
    let container = document.getElementById("addon-install-confirmation-content");
    is(container.childNodes.length, 1, "Should be one item listed");
    is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");

    progressPromise = waitForProgressNotification(true, 2);
    triggers = encodeURIComponent(JSON.stringify({
      "Theme XPI": "theme.xpi"
    }));
    gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;

    // Should still have the right add-on in the confirmation notification
    is(container.childNodes.length, 1, "Should be one item listed");
    is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");

    // Wait for the install to complete, we won't see a new confirmation
    // notification
    yield new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "addon-install-confirmation");
        resolve();
      }, "addon-install-confirmation", false);
    });

    // Make sure browser-addons.js executes first
    yield new Promise(resolve => executeSoon(resolve));

    // Should have dropped the progress notification
    is(PopupNotifications.panel.childNodes.length, 1, "Should be the right number of notifications");
    is(PopupNotifications.panel.childNodes[0].id, "addon-install-confirmation-notification",
       "Should only be showing one install confirmation");

    // Should still have the right add-on in the confirmation notification
    is(container.childNodes.length, 1, "Should be one item listed");
    is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");

    cancelInstallDialog(installDialog);

    ok(PopupNotifications.isPanelOpen, "Panel should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be the right number of notifications");
    is(PopupNotifications.panel.childNodes[0].id, "addon-install-confirmation-notification",
       "Should still have an install confirmation open");

    // Should have the next add-on's confirmation dialog
    is(container.childNodes.length, 1, "Should be one item listed");
    is(container.childNodes[0].firstChild.getAttribute("value"), "Theme Test", "Should have the right add-on");

    Services.perms.remove(makeURI("http://example.com"), "install");
    let closePromise = waitForNotificationClose();
    cancelInstallDialog(installDialog);
    yield closePromise;
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
},

function test_allUnverified() {
  return Task.spawn(function* () {
    // This test is only relevant if using the new doorhanger UI and allowing
    // unsigned add-ons
    if (!Preferences.get("xpinstall.customConfirmationUI", false) ||
        Preferences.get("xpinstall.signatures.required", true) ||
        REQUIRE_SIGNING) {
      return;
    }
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "Extension XPI": "restartless-unsigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let installDialog = yield dialogPromise;

    let notification = document.getElementById("addon-install-confirmation-notification");
    let message = notification.getAttribute("label");
    is(message, "Caution: This site would like to install an unverified add-on in " + gApp + ". Proceed at your own risk.");

    let container = document.getElementById("addon-install-confirmation-content");
    is(container.childNodes.length, 1, "Should be one item listed");
    is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");
    is(container.childNodes[0].childNodes.length, 1, "Shouldn't have the unverified marker");

    let notificationPromise = waitForNotification("addon-install-complete");
    acceptInstallDialog(installDialog);
    yield notificationPromise;

    let addon = yield new Promise(resolve => {
      AddonManager.getAddonByID("restartless-xpi@tests.mozilla.org", function(result) {
        resolve(result);
      });
    });
    addon.uninstall();

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_url() {
  return Task.spawn(function* () {
    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    gBrowser.selectedTab = gBrowser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gBrowser.loadURI(TESTROOT + "amosigned.xpi");
    yield progressPromise;
    let installDialog = yield dialogPromise;

    let notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.button.label, "Restart Now", "Should have seen the right button");
    is(notification.getAttribute("label"),
       "XPI Test will be installed after you restart " + gApp + ".",
       "Should have seen the right message");

    let installs = yield getInstalls();
    is(installs.length, 1, "Should be one pending install");
    installs[0].cancel();

    yield removeTab();
  });
},

function test_localFile() {
  return Task.spawn(function* () {
    let cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                       .getService(Components.interfaces.nsIChromeRegistry);
    let path;
    try {
      path = cr.convertChromeURL(makeURI(CHROMEROOT + "corrupt.xpi")).spec;
    } catch (ex) {
      path = CHROMEROOT + "corrupt.xpi";
    }

    let failPromise = new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "addon-install-failed");
        resolve();
      }, "addon-install-failed", false);
    });
    gBrowser.selectedTab = gBrowser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gBrowser.loadURI(path);
    yield failPromise;

    // Wait for the browser code to add the failure notification
    yield waitForSingleNotification();

    let notification = PopupNotifications.panel.childNodes[0];
    is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
    is(notification.getAttribute("label"),
       "This add-on could not be installed because it appears to be corrupt.",
       "Should have seen the right message");

    yield removeTab();
  });
},

function test_tabClose() {
  return Task.spawn(function* () {
    if (!Preferences.get("xpinstall.customConfirmationUI", false)) {
      runNextTest();
      return;
    }

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    gBrowser.selectedTab = gBrowser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gBrowser.loadURI(TESTROOT + "amosigned.xpi");
    yield progressPromise;
    yield dialogPromise;

    let installs = yield getInstalls();
    is(installs.length, 1, "Should be one pending install");

    let closePromise = waitForNotificationClose();
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
    yield closePromise;

    installs = yield getInstalls();
    is(installs.length, 0, "Should be no pending install since the tab is closed");
  });
},

// Add-ons should be cancelled and the install notification destroyed when
// navigating to a new origin
function test_tabNavigate() {
  return Task.spawn(function* () {
    if (!Preferences.get("xpinstall.customConfirmationUI", false)) {
      return;
    }
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "Extension XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    yield dialogPromise;

    let closePromise = waitForNotificationClose();
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gBrowser.loadURI("about:blank");
    yield closePromise;

    let installs = yield getInstalls();
    is(installs.length, 0, "Should be no pending install");

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield loadPromise;
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
},

function test_urlBar() {
  return Task.spawn(function* () {
    let notificationPromise = waitForNotification("addon-install-origin-blocked");
    gBrowser.selectedTab = gBrowser.addTab("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gURLBar.value = TESTROOT + "amosigned.xpi";
    gURLBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {});
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    ok(!notification.hasAttribute("buttonlabel"), "Button to allow install should be hidden.");
    yield removeTab();
  });
},

function test_wrongHost() {
  return Task.spawn(function* () {
    let requestedUrl = TESTROOT2 + "enabled.html";
    gBrowser.selectedTab = gBrowser.addTab();

    let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, requestedUrl);
    gBrowser.loadURI(TESTROOT2 + "enabled.html");
    yield loadedPromise;

    let progressPromise = waitForProgressNotification();
    let notificationPromise = waitForNotification("addon-install-failed");
    gBrowser.loadURI(TESTROOT + "corrupt.xpi");
    yield progressPromise;
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.getAttribute("label"),
       "The add-on downloaded from this site could not be installed " +
       "because it appears to be corrupt.",
       "Should have seen the right message");

    yield removeTab();
  });
},

function test_reload() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "Unsigned XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let installDialog = yield dialogPromise;

    let notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.button.label, "Restart Now", "Should have seen the right button");
    is(notification.getAttribute("label"),
       "XPI Test will be installed after you restart " + gApp + ".",
       "Should have seen the right message");

    function testFail() {
      ok(false, "Reloading should not have hidden the notification");
    }
    PopupNotifications.panel.addEventListener("popuphiding", testFail, false);
    let requestedUrl = TESTROOT2 + "enabled.html";
    let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, requestedUrl);
    gBrowser.loadURI(TESTROOT2 + "enabled.html");
    yield loadedPromise;
    PopupNotifications.panel.removeEventListener("popuphiding", testFail, false);

    let installs = yield getInstalls();
    is(installs.length, 1, "Should be one pending install");
    installs[0].cancel();

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_theme() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "Theme XPI": "theme.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let installDialog = yield dialogPromise;

    let notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    is(notification.button.label, "Restart Now", "Should have seen the right button");
    is(notification.getAttribute("label"),
       "Theme Test will be installed after you restart " + gApp + ".",
       "Should have seen the right message");

    let addon = yield new Promise(resolve => {
      AddonManager.getAddonByID("{972ce4c6-7e08-4474-a285-3208198ce6fd}", function(result) {
        resolve(result);
      });
    });
    ok(addon.userDisabled, "Should be switching away from the default theme.");
    // Undo the pending theme switch
    addon.userDisabled = false;

    addon = yield new Promise(resolve => {
      AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(result) {
        resolve(result);
      });
    });
    isnot(addon, null, "Test theme will have been installed");
    addon.uninstall();

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_renotifyBlocked() {
  return Task.spawn(function* () {
    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    let panel = yield notificationPromise;

    let closePromise = waitForNotificationClose();
    // hide the panel (this simulates the user dismissing it)
    panel.hidePopup();
    yield closePromise;

    info("Timeouts after this probably mean bug 589954 regressed");

    yield new Promise(resolve => executeSoon(resolve));

    notificationPromise = waitForNotification("addon-install-blocked");
    gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
    yield notificationPromise;

    let installs = yield getInstalls();
    is(installs.length, 2, "Should be two pending installs");

    closePromise = waitForNotificationClose();
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
    yield closePromise;

    installs = yield getInstalls();
    is(installs.length, 0, "Should have cancelled the installs");
  });
},

function test_renotifyInstalled() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    let installDialog = yield dialogPromise;

    // Wait for the complete notification
    let notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    let panel = yield notificationPromise;

    let closePromise = waitForNotificationClose();
    // hide the panel (this simulates the user dismissing it)
    panel.hidePopup();
    yield closePromise;

    // Install another
    yield new Promise(resolve => executeSoon(resolve));

    progressPromise = waitForProgressNotification();
    dialogPromise = waitForInstallDialog();
    gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
    yield progressPromise;
    installDialog = yield dialogPromise;

    info("Timeouts after this probably mean bug 589954 regressed");

    // Wait for the complete notification
    notificationPromise = waitForNotification("addon-install-restart");
    acceptInstallDialog(installDialog);
    yield notificationPromise;

    let installs = yield getInstalls();
    is(installs.length, 1, "Should be one pending installs");
    installs[0].cancel();

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield removeTab();
  });
},

function test_cancel() {
  return Task.spawn(function* () {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let notificationPromise = waitForNotification(PROGRESS_NOTIFICATION);
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "slowinstall.sjs?file=amosigned.xpi"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, TESTROOT + "installtrigger.html?" + triggers);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    // Close the notification
    let anchor = document.getElementById("addons-notification-icon");
    anchor.click();
    // Reopen the notification
    anchor.click();

    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    notification = panel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Cancel the download
    let install = notification.notification.options.installs[0];
    let cancelledPromise = new Promise(resolve => {
      install.addListener({
        onDownloadCancelled() {
          install.removeListener(this);
          resolve();
        }
      });
    });
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});
    yield cancelledPromise;

    yield new Promise(resolve => executeSoon(resolve));

    ok(!PopupNotifications.isPanelOpen, "Notification should be closed");

    let installs = yield getInstalls();
    is(installs.length, 0, "Should be no pending install");

    Services.perms.remove(makeURI("http://example.com/"), "install");
    yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
},

function test_failedSecurity() {
  return Task.spawn(function* () {
    Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);
    setupRedirect({
      "Location": TESTROOT + "amosigned.xpi"
    });

    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(JSON.stringify({
      "XPI": "redirect.sjs?mode=redirect"
    }));
    BrowserTestUtils.openNewForegroundTab(gBrowser, SECUREROOT + "installtrigger.html?" + triggers);
    let panel = yield notificationPromise;

    let notification = panel.childNodes[0];
    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Notification should have changed to progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    notification = panel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for it to fail
    yield new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "addon-install-failed");
        resolve();
      }, "addon-install-failed", false);
    });

    // Allow the browser code to add the failure notification and then wait
    // for the progress notification to dismiss itself
    yield waitForSingleNotification();
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    notification = panel.childNodes[0];
    is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");

    Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true);
    yield removeTab();
  });
}
];

var gTestStart = null;

var XPInstallObserver = {
  observe(aSubject, aTopic, aData) {
    var installInfo = aSubject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    info("Observed " + aTopic + " for " + installInfo.installs.length + " installs");
    installInfo.installs.forEach(function(aInstall) {
      info("Install of " + aInstall.sourceURI.spec + " was in state " + aInstall.state);
    });
  }
};

add_task(function* () {
  requestLongerTimeout(4);

  Services.prefs.setBoolPref("extensions.logging.enabled", true);
  Services.prefs.setBoolPref("extensions.strictCompatibility", true);
  Services.prefs.setBoolPref("extensions.install.requireSecureOrigin", false);
  Services.prefs.setIntPref("security.dialog_enable_delay", 0);

  Services.obs.addObserver(XPInstallObserver, "addon-install-started", false);
  Services.obs.addObserver(XPInstallObserver, "addon-install-blocked", false);
  Services.obs.addObserver(XPInstallObserver, "addon-install-failed", false);
  Services.obs.addObserver(XPInstallObserver, "addon-install-complete", false);

  registerCleanupFunction(function() {
    // Make sure no more test parts run in case we were timed out
    TESTS = [];

    AddonManager.getAllInstalls(function(aInstalls) {
      aInstalls.forEach(function(aInstall) {
        aInstall.cancel();
      });
    });

    Services.prefs.clearUserPref("extensions.logging.enabled");
    Services.prefs.clearUserPref("extensions.strictCompatibility");
    Services.prefs.clearUserPref("extensions.install.requireSecureOrigin");
    Services.prefs.clearUserPref("security.dialog_enable_delay");

    Services.obs.removeObserver(XPInstallObserver, "addon-install-started");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-blocked");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-failed");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-complete");
  });

  for (let i = 0; i < TESTS.length; ++i) {
    if (gTestStart)
      info("Test part took " + (Date.now() - gTestStart) + "ms");

    ok(!PopupNotifications.isPanelOpen, "Notification should be closed");

    let installs = yield new Promise(resolve => {
      AddonManager.getAllInstalls(function(aInstalls) {
        resolve(aInstalls);
      });
    });

    is(installs.length, 0, "Should be no active installs");
    info("Running " + TESTS[i].name);
    gTestStart = Date.now();
    yield TESTS[i]();
  }
});
