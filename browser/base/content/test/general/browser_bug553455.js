/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const TESTROOT2 = "http://example.org/browser/toolkit/mozapps/extensions/test/xpinstall/";
const SECUREROOT = "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";
const PREF_INSTALL_REQUIREBUILTINCERTS = "extensions.install.requireBuiltInCerts";

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
var check_notification;

function wait_for_notification(aCallback) {
  info("Waiting for notification");
  check_notification = function() {
    PopupNotifications.panel.removeEventListener("popupshown", check_notification, false);
    info("Saw notification");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    aCallback(PopupNotifications.panel);
  };
  PopupNotifications.panel.addEventListener("popupshown", check_notification, false);
}

function wait_for_notification_close(aCallback) {
  info("Waiting for notification to close");
  PopupNotifications.panel.addEventListener("popuphidden", function() {
    PopupNotifications.panel.removeEventListener("popuphidden", arguments.callee, false);
    aCallback();
  }, false);
}

function wait_for_install_dialog(aCallback) {
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

        aCallback(domwindow);
      }, domwindow);
    },

    onCloseWindow: function(aXULWindow) {
    },

    onWindowTitleChange: function(aXULWindow, aNewTitle) {
    }
  });
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "xpinstall-disabled-notification", "Should have seen installs disabled");
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
        is(aInstalls.length, 1, "Should have been one install created");
        aInstalls[0].cancel();

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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-blocked-notification", "Should have seen the install blocked");
    is(notification.button.label, "Allow", "Should have seen the right button");
    is(notification.getAttribute("label"),
       gApp + " prevented this site (example.com) from asking you to install " +
       "software on your computer.",
       "Should have seen the right message");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
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

function test_failed_download() {
  // Wait for the progress notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the failed notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the failed notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the failed notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
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

function test_url() {
  // Wait for the progress notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
    });
  });

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "unsigned.xpi");
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
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(path);
},

function test_wronghost() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.addEventListener("load", function() {
    if (gBrowser.currentURI.spec != TESTROOT2 + "enabled.html")
      return;

    gBrowser.removeEventListener("load", arguments.callee, true);

    // Wait for the progress notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-progress-notification", "Should have seen the progress notification");
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-blocked-notification", "Should have seen the install blocked");

    wait_for_notification_close(function () {
      info("Timeouts after this probably mean bug 589954 regressed");
      executeSoon(function () {
        wait_for_notification(function(aPanel) {
          let notification = aPanel.childNodes[0];
          is(notification.id, "addon-install-blocked-notification",
             "Should have seen the install blocked - 2nd time");

          AddonManager.getAllInstalls(function(aInstalls) {
          is(aInstalls.length, 2, "Should be two pending installs");
            aInstalls[0].cancel();
            aInstalls[1].cancel();

            info("Closing browser tab");
            wait_for_notification_close(runNextTest);
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");

        // Dismiss the notification
        wait_for_notification_close(function () {
          // Install another
          executeSoon(function () {
            // Wait for the progress notification
            wait_for_notification(function(aPanel) {
              let notification = aPanel.childNodes[0];
              is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

              // Wait for the install confirmation dialog
              wait_for_install_dialog(function(aWindow) {
                info("Timeouts after this probably mean bug 589954 regressed");

                // Wait for the complete notification
                wait_for_notification(function(aPanel) {
                  let notification = aPanel.childNodes[0];
                  is(notification.id, "addon-install-complete-notification", "Should have seen the second install complete");

                  AddonManager.getAllInstalls(function(aInstalls) {
                  is(aInstalls.length, 1, "Should be one pending installs");
                    aInstalls[0].cancel();

                    Services.perms.remove("example.com", "install");
                    wait_for_notification_close(runNextTest);
                    gBrowser.removeTab(gBrowser.selectedTab);
                  });
                });

                aWindow.document.documentElement.acceptDialog();
              });
            });

            gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);
          });
        });

        // hide the panel (this simulates the user dismissing it)
        aPanel.hidePopup();
      });

      aWindow.document.documentElement.acceptDialog();
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

function test_cancel_restart() {
  // Wait for the progress notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");

    // Close the notification
    let anchor = document.getElementById("addons-notification-icon");
    anchor.click();
    // Reopen the notification
    anchor.click();

    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    isnot(notification, aPanel.childNodes[0], "Should have reconstructed the notification UI");
    notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");
    let button = document.getAnonymousElementByAttribute(notification, "anonid", "cancel");

    // Cancel the download
    EventUtils.synthesizeMouse(button, 2, 2, {});

    // Notification should have changed to cancelled
    notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-cancelled-notification", "Should have seen the cancelled notification");

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
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

      aWindow.document.documentElement.acceptDialog();
    });

    // Restart the download
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Should be back to a progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(PopupNotifications.panel.childNodes.length, 1, "Should be only one notification");
    notification = aPanel.childNodes[0];
    is(notification.id, "addon-progress-notification", "Should have seen the progress notification");
  });

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
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
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-blocked-notification", "Should have seen the install blocked");

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

  Services.obs.addObserver(XPInstallObserver, "addon-install-started", false);
  Services.obs.addObserver(XPInstallObserver, "addon-install-blocked", false);
  Services.obs.addObserver(XPInstallObserver, "addon-install-failed", false);
  Services.obs.addObserver(XPInstallObserver, "addon-install-complete", false);

  registerCleanupFunction(function() {
    // Make sure no more test parts run in case we were timed out
    TESTS = [];
    PopupNotifications.panel.removeEventListener("popupshown", check_notification, false);

    AddonManager.getAllInstalls(function(aInstalls) {
      aInstalls.forEach(function(aInstall) {
        aInstall.cancel();
      });
    });

    Services.prefs.clearUserPref("extensions.logging.enabled");
    Services.prefs.clearUserPref("extensions.strictCompatibility");

    Services.obs.removeObserver(XPInstallObserver, "addon-install-started");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-blocked");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-failed");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-complete");
  });

  runNextTest();
}
