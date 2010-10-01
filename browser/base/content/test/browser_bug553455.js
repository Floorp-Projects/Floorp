/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const TESTROOT2 = "http://example.org/browser/toolkit/mozapps/extensions/test/xpinstall/";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";

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

function wait_for_notification(aCallback) {
  PopupNotifications.panel.addEventListener("popupshown", function() {
    PopupNotifications.panel.removeEventListener("popupshown", arguments.callee, false);
    aCallback(PopupNotifications.panel);
  }, false);
}

function wait_for_install_dialog(aCallback) {
  Services.wm.addListener({
    onOpenWindow: function(aXULWindow) {
      Services.wm.removeListener(this);

      var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowInternal);
      domwindow.addEventListener("load", function() {
        domwindow.removeEventListener("load", arguments.callee, false);

        is(domwindow.document.location.href, XPINSTALL_URL, "Should have seen the right window open");

        // Allow other window load listeners to execute before passing to callback
        executeSoon(function() {
          // Override the countdown timer on the accept button
          var button = domwindow.document.documentElement.getButton("accept");
          button.disabled = false;

          aCallback(domwindow);
        });
      }, false);
    },

    onCloseWindow: function(aXULWindow) {
    },

    onWindowTitleChange: function(aXULWindow, aNewTitle) {
    }
  });
}

var TESTS = [
function test_blocked_install() {
  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the blocked notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-blocked-notification", "Should have seen the install blocked");
    is(notification.button.label, "Allow", "Should have seen the right button");
    is(notification.getAttribute("label"),
       gApp + " prevented this site (example.com) from asking you to install " +
       "software on your computer.",
       "Should have seen the right message");

    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      aWindow.document.documentElement.acceptDialog();

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

          gBrowser.removeTab(gBrowser.selectedTab);
          runNextTest();
        });
      });
    });
  });
},

function test_whitelisted_install() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

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

        gBrowser.removeTab(gBrowser.selectedTab);
        Services.perms.remove("example.com", "install");
        runNextTest();
      });
    });
  });
},

function test_failed_download() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "missing.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the failed notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
    is(notification.getAttribute("label"),
       "The add-on could not be downloaded because of a connection failure " +
       "on example.com.",
       "Should have seen the right message");

    gBrowser.removeTab(gBrowser.selectedTab);
    Services.perms.remove("example.com", "install");
    runNextTest();
  });
},

function test_corrupt_file() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "corrupt.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the failed notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
    is(notification.getAttribute("label"),
       "The add-on downloaded from example.com could not be installed " +
       "because it appears to be corrupt.",
       "Should have seen the right message");

    gBrowser.removeTab(gBrowser.selectedTab);
    Services.perms.remove("example.com", "install");
    runNextTest();
  });
},

function test_incompatible() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "incompatible.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the failed notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
    is(notification.getAttribute("label"),
       "XPI Test could not be installed because it is not compatible with " +
       gApp + " " + gVersion + ".",
       "Should have seen the right message");

    gBrowser.removeTab(gBrowser.selectedTab);
    Services.perms.remove("example.com", "install");
    runNextTest();
  });
},

function test_restartless() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "restartless.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

    // Wait for the complete notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
      is(notification.button.label, "Open Add-ons Manager", "Should have seen the right button");
      is(notification.getAttribute("label"),
         "XPI Test has been installed successfully.",
         "Should have seen the right message");

      AddonManager.getAllInstalls(function(aInstalls) {
        is(aInstalls.length, 0, "Should be no pending installs");

        AddonManager.getAddonByID("restartless-xpi@tests.mozilla.org", function(aAddon) {
          aAddon.uninstall();

          gBrowser.removeTab(gBrowser.selectedTab);
          Services.perms.remove("example.com", "install");
          runNextTest();
        });
      });
    });
  });
},

function test_multiple() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": "unsigned.xpi",
    "Restartless XPI": "restartless.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

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

          gBrowser.removeTab(gBrowser.selectedTab);
          Services.perms.remove("example.com", "install");
          runNextTest();
        });
      });
    });
  });
},

function test_url() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "unsigned.xpi");

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

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

        gBrowser.removeTab(gBrowser.selectedTab);
        runNextTest();
      });
    });
  });
},

function test_localfile() {
  var cr = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIChromeRegistry);
  try {
    var path = cr.convertChromeURL(makeURI(CHROMEROOT + "corrupt.xpi")).spec;
  } catch (ex) {
    var path = CHROMEROOT + "corrupt.xpi";
  }
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(path);

  // Wait for the complete notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
    is(notification.getAttribute("label"),
       "This add-on could not be installed because it appears to be corrupt.",
       "Should have seen the right message");

    gBrowser.removeTab(gBrowser.selectedTab);
    runNextTest();
  });
},

function test_wronghost() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.addEventListener("load", function() {
    if (gBrowser.currentURI.spec != TESTROOT2 + "enabled.html")
      return;

    gBrowser.removeEventListener("load", arguments.callee, true);

    gBrowser.loadURI(TESTROOT + "corrupt.xpi");

    // Wait for the complete notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-failed-notification", "Should have seen the install fail");
      is(notification.getAttribute("label"),
         "The add-on downloaded from example.com could not be installed " +
         "because it appears to be corrupt.",
         "Should have seen the right message");

      gBrowser.removeTab(gBrowser.selectedTab);
      runNextTest();
    });
  }, true);
  gBrowser.loadURI(TESTROOT2 + "enabled.html");
},

function test_reload() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Unsigned XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

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

          gBrowser.removeTab(gBrowser.selectedTab);
          Services.perms.remove("example.com", "install");
          runNextTest();
        });
      }, true);
      gBrowser.loadURI(TESTROOT2 + "enabled.html");
    });
  });
},

function test_theme() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "Theme XPI": "theme.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

    // Wait for the complete notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");
      is(notification.button.label, "Restart Now", "Should have seen the right button");
      is(notification.getAttribute("label"),
         "Theme Test will be installed after you restart " + gApp + ".",
         "Should have seen the right message");

      gBrowser.removeTab(gBrowser.selectedTab);
      Services.perms.remove("example.com", "install");

      AddonManager.getAddonByID("{972ce4c6-7e08-4474-a285-3208198ce6fd}", function(aAddon) {
        ok(aAddon.userDisabled, "Should be switching away from the default theme.");
        // Undo the pending theme switch
        aAddon.userDisabled = false;

        AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(aAddon) {
          isnot(aAddon, null, "Test theme will have been installed");
          aAddon.uninstall();

          runNextTest();
        });
      });
    });
  });
},

function test_renotify_blocked() {
  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the blocked notification
  wait_for_notification(function(aPanel) {
    let notification = aPanel.childNodes[0];
    is(notification.id, "addon-install-blocked-notification", "Should have seen the install blocked");

    aPanel.addEventListener("popuphidden", function () {
      aPanel.removeEventListener("popuphidden", arguments.callee, false);
      info("Timeouts after this probably mean bug 589954 regressed");
      executeSoon(function () {
        gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

        wait_for_notification(function(aPanel) {
          let notification = aPanel.childNodes[0];
          is(notification.id, "addon-install-blocked-notification",
             "Should have seen the install blocked - 2nd time");

          AddonManager.getAllInstalls(function(aInstalls) {
          is(aInstalls.length, 2, "Should be two pending installs");
            aInstalls[0].cancel();
            aInstalls[1].cancel();

            info("Closing browser tab");
            gBrowser.removeTab(gBrowser.selectedTab);
            runNextTest();
          });
        });

      });
    }, false);

    // hide the panel (this simulates the user dismissing it)
    aPanel.hidePopup();
  });
},

function test_renotify_installed() {
  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  var triggers = encodeURIComponent(JSON.stringify({
    "XPI": "unsigned.xpi"
  }));
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

  // Wait for the install confirmation dialog
  wait_for_install_dialog(function(aWindow) {
    aWindow.document.documentElement.acceptDialog();

    // Wait for the complete notification
    wait_for_notification(function(aPanel) {
      let notification = aPanel.childNodes[0];
      is(notification.id, "addon-install-complete-notification", "Should have seen the install complete");

      // Dismiss the notification
      aPanel.addEventListener("popuphidden", function () {
        aPanel.removeEventListener("popuphidden", arguments.callee, false);

        // Install another
        executeSoon(function () {
          gBrowser.loadURI(TESTROOT + "installtrigger.html?" + triggers);

          // Wait for the install confirmation dialog
          wait_for_install_dialog(function(aWindow) {
            aWindow.document.documentElement.acceptDialog();
            info("Timeouts after this probably mean bug 589954 regressed");

            // Wait for the complete notification
            wait_for_notification(function(aPanel) {
              let notification = aPanel.childNodes[0];
              is(notification.id, "addon-install-complete-notification", "Should have seen the second install complete");

              AddonManager.getAllInstalls(function(aInstalls) {
              is(aInstalls.length, 2, "Should be two pending installs");
                aInstalls[0].cancel();
                aInstalls[1].cancel();

                gBrowser.removeTab(gBrowser.selectedTab);
                runNextTest();
              });
            });
          });

        });
      }, false);
  
      // hide the panel (this simulates the user dismissing it)
      aPanel.hidePopup();
    });
  });
}
];

function runNextTest() {
  AddonManager.getAllInstalls(function(aInstalls) {
    is(aInstalls.length, 0, "Should be no active installs");

    if (TESTS.length == 0) {
      Services.prefs.setBoolPref("extensions.logging.enabled", false);

      finish();
      return;
    }

    info("Running " + TESTS[0].name);
    TESTS.shift()();
  });
};

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("extensions.logging.enabled", true);

  runNextTest();
}
