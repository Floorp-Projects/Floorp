/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const TESTROOT2 = "http://example.org/browser/toolkit/mozapps/extensions/test/xpinstall/";
const SECUREROOT = "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";
const PROGRESS_NOTIFICATION = "addon-progress";

var rootDir = getRootDirectory(gTestPath);
var path = rootDir.split('/');
var chromeName = path[0] + '//' + path[2];
var croot = chromeName + "/content/browser/toolkit/mozapps/extensions/test/xpinstall/";
var jar = getJar(croot);
if (jar) {
  var tmpdir = extractJarToTmp(jar);
  croot = 'file://' + tmpdir.path + '/';
}
const CHROMEROOT = croot;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;

function get_observer_topic(aNotificationId) {
  let topic = aNotificationId;
  if (topic == "xpinstall-disabled")
    topic = "addon-install-disabled";
  else if (topic == "addon-progress")
    topic = "addon-install-started";
  return topic;
}

function wait_for_progress_notification(aCallback, aExpectedCount = 1) {
  wait_for_notification(PROGRESS_NOTIFICATION, aCallback, aExpectedCount, "popupshowing");
}

function wait_for_notification(aId, aCallback, aExpectedCount = 1, aEvent = "popupshown") {
  info("Waiting for " + aId + " notification");

  let topic = get_observer_topic(aId);
  function observer(aSubject, aTopic, aData) {
    // Ignore the progress notification unless that is the notification we want
    if (aId != PROGRESS_NOTIFICATION &&
        aTopic == get_observer_topic(PROGRESS_NOTIFICATION))
      return;

    Services.obs.removeObserver(observer, topic);

    if (PopupNotifications.isPanelOpen)
      executeSoon(verify);
    else
      PopupNotifications.panel.addEventListener(aEvent, event_listener);
  }

  function event_listener() {
    // Ignore the progress notification unless that is the notification we want
    if (aId != PROGRESS_NOTIFICATION &&
        PopupNotifications.panel.childNodes[0].id == PROGRESS_NOTIFICATION + "-notification")
      return;

    PopupNotifications.panel.removeEventListener(aEvent, event_listener);

    verify();
  }

  function verify() {
    info("Saw a notification");
    ok(PopupNotifications.isPanelOpen, "Panel should be open");
    is(PopupNotifications.panel.childNodes.length, aExpectedCount, "Should be the right number of notifications");
    if (PopupNotifications.panel.childNodes.length) {
      let nodes = Array.from(PopupNotifications.panel.childNodes);
      let notification = nodes.find(n => n.id == aId + "-notification");
      ok(notification, "Should have seen the right notification");
    }
    aCallback(PopupNotifications.panel);
  }

  Services.obs.addObserver(observer, topic, false);
}

function wait_for_notification_close(aCallback) {
  info("Waiting for notification to close");
  PopupNotifications.panel.addEventListener("popuphidden", function() {
    PopupNotifications.panel.removeEventListener("popuphidden", arguments.callee, false);
    aCallback();
  }, false);
}

function wait_for_install_dialog(aCallback) {
  if (Preferences.get("xpinstall.customConfirmationUI", false)) {
    wait_for_notification("addon-install-confirmation", function(aPanel) {
      aCallback();
    });
    return;
  }

  info("Waiting for install dialog");

  Services.wm.addListener({
    onOpenWindow: function(aXULWindow) {
      info("Install dialog opened, waiting for focus");
      Services.wm.removeListener(this);

      var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow);
      waitForFocus(function() {
        info("Saw install dialog");
        is(domwindow.document.location.href, XPINSTALL_URL, "Should have seen the right window open");

        // Override the countdown timer on the accept button
        var button = domwindow.document.documentElement.getButton("accept");
        button.disabled = false;

        aCallback();
      }, domwindow);
    },

    onCloseWindow: function(aXULWindow) {
    },

    onWindowTitleChange: function(aXULWindow, aNewTitle) {
    }
  });
}

function accept_install_dialog() {
  if (Preferences.get("xpinstall.customConfirmationUI", false)) {
    document.getElementById("addon-install-confirmation-accept").click();
  } else {
    let win = Services.wm.getMostRecentWindow("Addons:Install");
    win.document.documentElement.acceptDialog();
  }
}

function cancel_install_dialog() {
  if (Preferences.get("xpinstall.customConfirmationUI", false)) {
    document.getElementById("addon-install-confirmation-cancel").click();
  } else {
    let win = Services.wm.getMostRecentWindow("Addons:Install");
    win.document.documentElement.cancelDialog();
  }
}

function wait_for_single_notification(aCallback) {
  function inner_waiter() {
    info("Waiting for single notification");
    // Notification should never close while we wait
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    if (PopupNotifications.panel.childNodes.length == 2) {
      executeSoon(inner_waiter);
      return;
    }

    aCallback();
  }

  executeSoon(inner_waiter);
}

function setup_redirect(aSettings) {
  var url = "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/redirect.sjs?mode=setup";
  for (var name in aSettings) {
    url += "&" + name + "=" + aSettings[name];
  }

  var req = new XMLHttpRequest();
  req.open("GET", url, false);
  req.send(null);
}

var TESTS = [
function test_disabled_install() {
  Services.prefs.setBoolPref("xpinstall.enabled", false);

  // Wait for the disabled notification
  wait_for_notification("xpinstall-disabled", function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.button.label, "Enable", "Should have seen the right button");
    is(notification.getAttribute("label"),
       "Software installation is currently disabled. Click Enable and try again.");

    wait_for_notification_close(function() {
      try {
        ok(Services.prefs.getBoolPref("xpinstall.enabled"), "Installation should be enabled");
      }
      catch (e) {
        ok(false, "xpinstall.enabled should be set");
      }

      gBrowser.removeTab(gBrowser.selectedTab);

      AddonManager.getAllInstalls(function(aInstalls) {
        is(aInstalls.length, 0, "Shouldn't be any pending installs");

        runNextTest();
      });
    });

    // Click on Enable
    EventUtils.synthesizeMouseAtCenter(notification.button, {});
  });

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_blocked_install() {
  // Wait for the blocked notification
  wait_for_notification("addon-install-blocked", function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.button.label, "Allow", "Should have seen the right button");
    is(notification.getAttribute("origin"), "example.com",
       "Should have seen the right origin host");
    is(notification.getAttribute("label"),
       gApp + " prevented this site from asking you to install software on your computer.",
       "Should have seen the right message");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.button.label, "Restart Now", "Should have seen the right button");
        is(notification.getAttribute("label"),
           "XPI Test will be installed after you restart " + gApp + ".",
           "Should have seen the right message");

        AddonManager.getAllInstalls(function(aInstalls) {
        is(aInstalls.length, 1, "Should be one pending install");
          aInstalls[0].cancel();

          wait_for_notification_close(runNextTest);
          gBrowser.removeTab(gBrowser.selectedTab);
        });
      });

      accept_install_dialog();
    });

    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Notification should have changed to progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");
  });

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_whitelisted_install() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    gBrowser.selectedTab = originalTab;

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      is(gBrowser.selectedTab, tab,
         "tab selected in response to the addon-install-confirmation notification");

      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.button.label, "Restart Now", "Should have seen the right button");
        is(notification.getAttribute("label"),
           "XPI Test will be installed after you restart " + gApp + ".",
           "Should have seen the right message");

        AddonManager.getAllInstalls(function(aInstalls) {
          is(aInstalls.length, 1, "Should be one pending install");
          aInstalls[0].cancel();

          Services.perms.remove("example.com", "install");
          wait_for_notification_close(runNextTest);
          gBrowser.removeTab(gBrowser.selectedTab);
        });
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  let originalTab = gBrowser.selectedTab;
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_failed_download() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the failed notification
    wait_for_notification("addon-install-failed", function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.getAttribute("label"),
         "The add-on could not be downloaded because of a connection failure " +
         "on example.com.",
         "Should have seen the right message");

      Services.perms.remove("example.com", "install");
      wait_for_notification_close(runNextTest);
      gBrowser.removeTab(gBrowser.selectedTab);
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "missing.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_corrupt_file() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the failed notification
    wait_for_notification("addon-install-failed", function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.getAttribute("label"),
         "The add-on downloaded from example.com could not be installed " +
         "because it appears to be corrupt.",
         "Should have seen the right message");

      Services.perms.remove("example.com", "install");
      wait_for_notification_close(runNextTest);
      gBrowser.removeTab(gBrowser.selectedTab);
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "corrupt.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_incompatible() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the failed notification
    wait_for_notification("addon-install-failed", function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.getAttribute("label"),
         "XPI Test could not be installed because it is not compatible with " +
         gApp + " " + gVersion + ".",
         "Should have seen the right message");

      Services.perms.remove("example.com", "install");
      wait_for_notification_close(runNextTest);
      gBrowser.removeTab(gBrowser.selectedTab);
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "incompatible.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_restartless() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.getAttribute("label"),
           "XPI Test has been installed successfully.",
           "Should have seen the right message");

        AddonManager.getAllInstalls(function(aInstalls) {
          is(aInstalls.length, 0, "Should be no pending installs");

          AddonManager.getAddonByID("restartless-xpi@tests.mozilla.org", function(aAddon) {
            aAddon.uninstall();

            Services.perms.remove("example.com", "install");
            wait_for_notification_close(runNextTest);
            gBrowser.removeTab(gBrowser.selectedTab);
          });
        });
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "restartless.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_multiple() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.button.label, "Restart Now", "Should have seen the right button");
        is(notification.getAttribute("label"),
           "2 add-ons will be installed after you restart " + gApp + ".",
           "Should have seen the right message");

        AddonManager.getAllInstalls(function(aInstalls) {
          is(aInstalls.length, 1, "Should be one pending install");
          aInstalls[0].cancel();

          AddonManager.getAddonByID("restartless-xpi@tests.mozilla.org", function(aAddon) {
            aAddon.uninstall();

            Services.perms.remove("example.com", "install");
            wait_for_notification_close(runNextTest);
            gBrowser.removeTab(gBrowser.selectedTab);
          });
        });
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": "unsigned.xpi",
    "Restartless XPI": "restartless.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_sequential() {
  // This test is only relevant if using the new doorhanger UI
  if (!Preferences.get("xpinstall.customConfirmationUI", false)) {
    runNextTest();
    return;
  }

  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the progress notification

      // Should see the right add-on
      let container = document.getElementById("addon-install-confirmation-content");
      is(container.childNodes.length, 1, "Should be one item listed");
      is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");

      wait_for_progress_notification(function(aPanel) {
        // Should still have the right add-on in the confirmation notification
        is(container.childNodes.length, 1, "Should be one item listed");
        is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");

        // Wait for the install to complete, we won't see a new confirmation
        // notification
        Services.obs.addObserver(function observer() {
          Services.obs.removeObserver(observer, "addon-install-confirmation");

          // Make sure browser-addons.js executes first
          executeSoon(function () {
            // Should have dropped the progress notification
            is(PopupNotifications.panel.childNodes.length, 1, "Should be the right number of notifications");
            is(PopupNotifications.panel.childNodes[0].id, "addon-install-confirmation-notification",
               "Should only be showing one install confirmation");

            // Should still have the right add-on in the confirmation notification
            is(container.childNodes.length, 1, "Should be one item listed");
            is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");

            cancel_install_dialog();

            ok(PopupNotifications.isPanelOpen, "Panel should still be open");
            is(PopupNotifications.panel.childNodes.length, 1, "Should be the right number of notifications");
            is(PopupNotifications.panel.childNodes[0].id, "addon-install-confirmation-notification",
               "Should still have an install confirmation open");

            // Should have the next add-on's confirmation dialog
            is(container.childNodes.length, 1, "Should be one item listed");
            is(container.childNodes[0].firstChild.getAttribute("value"), "Theme Test", "Should have the right add-on");

            Services.perms.remove("example.com", "install");
            wait_for_notification_close(() => {
              gBrowser.removeTab(gBrowser.selectedTab);
              runNextTest();
            });

            cancel_install_dialog();
          });
        }, "addon-install-confirmation", false);
      }, 2);

      var triggers = encodeURIComponent(JSON.stringify({
        "Theme XPI": "theme.xpi"
      }));
      gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Restartless XPI": "restartless.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_someunverified() {
  // This test is only relevant if using the new doorhanger UI and allowing
  // unsigned add-ons
  if (!Preferences.get("xpinstall.customConfirmationUI", false) ||
      Preferences.get("xpinstall.signatures.required", true)) {
    runNextTest();
    return;
  }

  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      let notification = document.getElementById("addon-install-confirmation-notification");
      let message = notification.getAttribute("label");
      is(message, "Caution: This site would like to install 2 add-ons in " + gApp + ", some of which are unverified. Proceed at your own risk.",
         "Should see the right message");

      let container = document.getElementById("addon-install-confirmation-content");
      is(container.childNodes.length, 2, "Should be two items listed");
      is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");
      is(container.childNodes[0].lastChild.getAttribute("class"),
         "addon-install-confirmation-unsigned", "Should have the unverified marker");
      is(container.childNodes[1].firstChild.getAttribute("value"), "Theme Test", "Should have the right add-on");
      is(container.childNodes[1].childNodes.length, 1, "Shouldn't have the unverified marker");

      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        AddonManager.getAddonsByIDs(["restartless-xpi@tests.mozilla.org",
                                     "theme-xpi@tests.mozilla.org"], function([a, t]) {
          a.uninstall();
          // Installing a new theme tries to switch to it, switch back to the
          // default theme.
          t.userDisabled = true;
          t.uninstall();

          Services.perms.remove("example.com", "install");
          wait_for_notification_close(runNextTest);
          gBrowser.removeTab(gBrowser.selectedTab);
        });
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Extension XPI": "restartless.xpi",
    "Theme XPI": "theme.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_allunverified() {
  // This test is only relevant if using the new doorhanger UI and allowing
  // unsigned add-ons
  if (!Preferences.get("xpinstall.customConfirmationUI", false) ||
      Preferences.get("xpinstall.signatures.required", true)) {
    runNextTest();
    return;
  }

  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      let notification = document.getElementById("addon-install-confirmation-notification");
      let message = notification.getAttribute("label");
      is(message, "Caution: This site would like to install an unverified add-on in " + gApp + ". Proceed at your own risk.");

      let container = document.getElementById("addon-install-confirmation-content");
      is(container.childNodes.length, 1, "Should be one item listed");
      is(container.childNodes[0].firstChild.getAttribute("value"), "XPI Test", "Should have the right add-on");
      is(container.childNodes[0].childNodes.length, 1, "Shouldn't have the unverified marker");

      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        AddonManager.getAddonByID("restartless-xpi@tests.mozilla.org", function(aAddon) {
          aAddon.uninstall();

          Services.perms.remove("example.com", "install");
          wait_for_notification_close(runNextTest);
          gBrowser.removeTab(gBrowser.selectedTab);
        });
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Extension XPI": "restartless.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_url() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.button.label, "Restart Now", "Should have seen the right button");
        is(notification.getAttribute("label"),
           "XPI Test will be installed after you restart " + gApp + ".",
           "Should have seen the right message");

        AddonManager.getAllInstalls(function(aInstalls) {
          is(aInstalls.length, 1, "Should be one pending install");
          aInstalls[0].cancel();

          wait_for_notification_close(runNextTest);
          gBrowser.removeTab(gBrowser.selectedTab);
        });
      });

      accept_install_dialog();
    });
  });

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    gBrowser.loadURI(TESTROOT + "unsigned.xpi");
  });
},

function test_localfile() {
  // Wait for the install to fail
  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, "addon-install-failed");

    // Wait for the browser code to add the failure notification
    wait_for_single_notification(function() {
      let notification = PopupNotifications.panel.childNodes[0];
      is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
      is(notification.getAttribute("label"),
         "This add-on could not be installed because it appears to be corrupt.",
         "Should have seen the right message");

      wait_for_notification_close(runNextTest);
      gBrowser.removeTab(gBrowser.selectedTab);
    });
  }, "addon-install-failed", false);

  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);
  try {
    var path = cr.convertChromeURL(makeURI(CHROMEROOT + "corrupt.xpi")).spec;
  } catch (ex) {
    var path = CHROMEROOT + "corrupt.xpi";
  }
  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    gBrowser.loadURI(path);
  });
},

function test_tabclose() {
  if (!Preferences.get("xpinstall.customConfirmationUI", false)) {
    runNextTest();
    return;
  }

  // Wait for the progress notification
  wait_for_progress_notification(aPanel => {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(() => {
      AddonManager.getAllInstalls(aInstalls => {
        is(aInstalls.length, 1, "Should be one pending install");

        wait_for_notification_close(() => {
          AddonManager.getAllInstalls(aInstalls => {
            is(aInstalls.length, 0, "Should be no pending install since the tab is closed");
            runNextTest();
          });
        });

        gBrowser.removeTab(gBrowser.selectedTab);
      });
    });
  });

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    gBrowser.loadURI(TESTROOT + "unsigned.xpi");
  });
},

// Add-ons should be cancelled and the install notification destroyed when
// navigating to a new origin
function test_tabnavigate() {
  if (!Preferences.get("xpinstall.customConfirmationUI", false)) {
    runNextTest();
    return;
  }

  // Wait for the progress notification
  wait_for_progress_notification(aPanel => {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(() => {
      wait_for_notification_close(() => {
        AddonManager.getAllInstalls(aInstalls => {
          is(aInstalls.length, 0, "Should be no pending install");

          Services.perms.remove("http://example.com/", "install");
          loadPromise.then(() => {
            gBrowser.removeTab(gBrowser.selectedTab);
            runNextTest();
          });
        });
      });

      let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      gBrowser.loadURI("about:blank");
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Extension XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_urlbar() {
  wait_for_notification("addon-install-origin-blocked", function(aPanel) {
    let notification = aPanel.childNodes[0];

    is(notification.button.label, "", "Button to allow install should be hidden.");

    wait_for_notification_close(() => {
      runNextTest();
    });

    gBrowser.removeCurrentTab();
  });

  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(() => {
    gURLBar.value = TESTROOT + "unsigned.xpi";
    gURLBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {});
  });
},

function test_wronghost() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.addEventListener("load", function() {
    if (gBrowser.currentURI.spec != TESTROOT2 + "enabled.html")
      return;

    gBrowser.removeEventListener("load", arguments.callee, true);

    // Wait for the progress notification
    wait_for_progress_notification(function(aPanel) {
      // Wait for the complete notification
      wait_for_notification("addon-install-failed", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.getAttribute("label"),
           "The add-on downloaded from example.com could not be installed " +
           "because it appears to be corrupt.",
           "Should have seen the right message");

        wait_for_notification_close(runNextTest);
        gBrowser.removeTab(gBrowser.selectedTab);
      });
    });

    gBrowser.loadURI(TESTROOT + "corrupt.xpi");
  }, true);
  gBrowser.loadURI(TESTROOT2 + "enabled.html");
},

function test_reload() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.button.label, "Restart Now", "Should have seen the right button");
        is(notification.getAttribute("label"),
           "XPI Test will be installed after you restart " + gApp + ".",
           "Should have seen the right message");

        function test_fail() {
          ok(false, "Reloading should not have hidden the notification");
        }

        PopupNotifications.panel.addEventListener("popuphiding", test_fail, false);

        gBrowser.addEventListener("load", function() {
          if (gBrowser.currentURI.spec != TESTROOT2 + "enabled.html")
            return;

          gBrowser.removeEventListener("load", arguments.callee, true);

          PopupNotifications.panel.removeEventListener("popuphiding", test_fail, false);

          AddonManager.getAllInstalls(function(aInstalls) {
            is(aInstalls.length, 1, "Should be one pending install");
            aInstalls[0].cancel();

            Services.perms.remove("example.com", "install");
            wait_for_notification_close(runNextTest);
            gBrowser.removeTab(gBrowser.selectedTab);
          });
        }, true);
        gBrowser.loadURI(TESTROOT2 + "enabled.html");
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_theme() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.button.label, "Restart Now", "Should have seen the right button");
        is(notification.getAttribute("label"),
           "Theme Test will be installed after you restart " + gApp + ".",
           "Should have seen the right message");

        AddonManager.getAddonByID("{972ce4c6-7e08-4474-a285-3208198ce6fd}", function(aAddon) {
          ok(aAddon.userDisabled, "Should be switching away from the default theme.");
          // Undo the pending theme switch
          aAddon.userDisabled = false;

          AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(aAddon) {
            isnot(aAddon, null, "Test theme will have been installed");
            aAddon.uninstall();

            Services.perms.remove("example.com", "install");
            wait_for_notification_close(runNextTest);
            gBrowser.removeTab(gBrowser.selectedTab);
          });
        });
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Theme XPI": "theme.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_renotify_blocked() {
  // Wait for the blocked notification
  wait_for_notification("addon-install-blocked", function(aPanel) {
    let notification = aPanel.childNodes[0];

    wait_for_notification_close(function () {
      info("Timeouts after this probably mean bug 589954 regressed");
      executeSoon(function () {
        wait_for_notification("addon-install-blocked", function(aPanel) {
          AddonManager.getAllInstalls(function(aInstalls) {
            is(aInstalls.length, 2, "Should be two pending installs");

            wait_for_notification_close(() => {
              AddonManager.getAllInstalls(function(aInstalls) {
                is(aInstalls.length, 0, "Should have cancelled the installs");
                runNextTest();
              });
            });

            info("Closing browser tab");
            gBrowser.removeTab(gBrowser.selectedTab);
          });
        });

        gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
      });
    });

    // hide the panel (this simulates the user dismissing it)
    aPanel.hidePopup();
  });

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_renotify_installed() {
  // Wait for the progress notification
  wait_for_progress_notification(function(aPanel) {
    // Wait for the install confirmation dialog
    wait_for_install_dialog(function() {
      // Wait for the complete notification
      wait_for_notification("addon-install-complete", function(aPanel) {
        // Dismiss the notification
        wait_for_notification_close(function () {
          // Install another
          executeSoon(function () {
            // Wait for the progress notification
            wait_for_progress_notification(function(aPanel) {
              // Wait for the install confirmation dialog
              wait_for_install_dialog(function() {
                info("Timeouts after this probably mean bug 589954 regressed");

                // Wait for the complete notification
                wait_for_notification("addon-install-complete", function(aPanel) {
                  AddonManager.getAllInstalls(function(aInstalls) {
                  is(aInstalls.length, 1, "Should be one pending installs");
                    aInstalls[0].cancel();

                    Services.perms.remove("example.com", "install");
                    wait_for_notification_close(runNextTest);
                    gBrowser.removeTab(gBrowser.selectedTab);
                  });
                });

                accept_install_dialog();
              });
            });

            gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
          });
        });

        // hide the panel (this simulates the user dismissing it)
        aPanel.hidePopup();
      });

      accept_install_dialog();
    });
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_cancel() {
  function complete_install(callback) {
    let url = TESTROOT + "slowinstall.sjs?continue=true"
    NetUtil.asyncFetch({
      uri: url,
      loadUsingSystemPrincipal: true
    }, callback || (() => {}));
  }

  // Wait for the progress notification
  wait_for_notification(PROGRESS_NOTIFICATION, function(aPanel) {
    let notification = aPanel.childNodes[0];
    // Close the notification
    let anchor = document.getElementById("addons-notification-icon");
    anchor.click();
    // Reopen the notification
    anchor.click();

    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");
    let button = document.getElementById("addon-progress-cancel");

    // Wait for the install to fully cancel
    let install = notification.notification.options.installs[0];
    install.addListener({
      onDownloadCancelled: function() {
        install.removeListener(this);

        executeSoon(function() {
          ok(!PopupNotifications.isPanelOpen, "Notification should be closed");

          AddonManager.getAllInstalls(function(aInstalls) {
            is(aInstalls.length, 0, "Should be no pending install");

            Services.perms.remove("example.com", "install");
            gBrowser.removeTab(gBrowser.selectedTab);
            runNextTest();
          });
        });
      }
    });

    // Cancel the download
    EventUtils.synthesizeMouseAtCenter(button, {});
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "slowinstall.sjs?file=unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
},

function test_failed_security() {
  Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);

  setup_redirect({
    "Location": TESTROOT + "unsigned.xpi"
  });

  // Wait for the blocked notification
  wait_for_notification("addon-install-blocked", function(aPanel) {
    let notification = aPanel.childNodes[0];

    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Notification should have changed to progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for it to fail
    Services.obs.addObserver(function() {
      Services.obs.removeObserver(arguments.callee, "addon-install-failed");

      // Allow the browser code to add the failure notification and then wait
      // for the progress notification to dismiss itself
      wait_for_single_notification(function() {
        is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
        notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");

        Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true);
        wait_for_notification_close(runNextTest);
        gBrowser.removeTab(gBrowser.selectedTab);
      });
    }, "addon-install-failed", false);
  });

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "redirect.sjs?mode=redirect"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(SECUREROOT + "installtrigger.html?" + triggers);
}
];

var gTestStart = null;

function runNextTest() {
  if (gTestStart)
    info("Test part took " + (Date.now() - gTestStart) + "ms");

  ok(!PopupNotifications.isPanelOpen, "Notification should be closed");

  AddonManager.getAllInstalls(function(aInstalls) {
    is(aInstalls.length, 0, "Should be no active installs");

    if (TESTS.length == 0) {
      finish();
      return;
    }

    info("Running " + TESTS[0].name);
    gTestStart = Date.now();
    TESTS.shift()();
  });
};

var XPInstallObserver = {
  observe: function (aSubject, aTopic, aData) {
    var installInfo = aSubject.QueryInterface(Components.interfaces.amIWebInstallInfo);
    info("Observed " + aTopic + " for " + installInfo.installs.length + " installs");
    installInfo.installs.forEach(function(aInstall) {
      info("Install of " + aInstall.sourceURI.spec + " was in state " + aInstall.state);
    });
  }
};

function test() {
  requestLongerTimeout(4);
  waitForExplicitFinish();

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

  runNextTest();
}
