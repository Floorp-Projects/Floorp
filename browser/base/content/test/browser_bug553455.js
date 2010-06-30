/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const XPINSTALL_URL = "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul";

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
    is(notification.id, "addon-install-blocked", "Should have seen the install blocked");
    is(notification.button.label, "Allow", "Should have seen the right button");

    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Wait for the install confirmation dialog
    wait_for_install_dialog(function(aWindow) {
      aWindow.document.documentElement.acceptDialog();

      // Wait for the complete notification
      wait_for_notification(function(aPanel) {
        let notification = aPanel.childNodes[0];
        is(notification.id, "addon-install-complete", "Should have seen the install complete");
        is(notification.button.label, "Restart Now", "Should have seen the right button");

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
      is(notification.id, "addon-install-complete", "Should have seen the install complete");
      is(notification.button.label, "Restart Now", "Should have seen the right button");

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
    is(notification.id, "addon-install-failed", "Should have seen the install fail");

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
    is(notification.id, "addon-install-failed", "Should have seen the install fail");

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
    is(notification.id, "addon-install-failed", "Should have seen the install fail");

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
      is(notification.id, "addon-install-complete", "Should have seen the install complete");
      is(notification.button.label, "Open Add-ons Manager", "Should have seen the right button");

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
      is(notification.id, "addon-install-complete", "Should have seen the install complete");
      is(notification.button.label, "Restart Now", "Should have seen the right button");

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
}
];

function runNextTest() {
  AddonManager.getAllInstalls(function(aInstalls) {
    is(aInstalls.length, 0, "Should be no active installs");

    if (TESTS.length == 0) {
      finish();
      return;
    }

    TESTS.shift()();
  });
}

function test() {
  waitForExplicitFinish();

  runNextTest();
}
