/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";

const {LightweightThemeManager} = Cu.import("resource://gre/modules/LightweightThemeManager.jsm", {});

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
function promisePopupNotificationShown(name) {
  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(name);
      if (!notification) { return; }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}


var TESTS = [
function test_install_http() {
  is(LightweightThemeManager.currentTheme, null, "Should be no lightweight theme selected");

  var pm = Services.perms;
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  // NB: Not https so no installs allowed.
  const URL = "http://example.org/browser/browser/base/content/test/general/bug592338.html";
  BrowserTestUtils.openNewForegroundTab({ gBrowser, url: URL }).then(async function() {
    let prompted = promisePopupNotificationShown("addon-webext-permissions");
    BrowserTestUtils.synthesizeMouse("#theme-install", 2, 2, {}, gBrowser.selectedBrowser);
    await prompted;

    is(LightweightThemeManager.currentTheme, null, "Should not have installed the test theme");

    gBrowser.removeTab(gBrowser.selectedTab);

    pm.remove(makeURI("http://example.org/"), "install");

    runNextTest();
  });
},

function test_install_lwtheme() {
  is(LightweightThemeManager.currentTheme, null, "Should be no lightweight theme selected");

  var pm = Services.perms;
  pm.add(makeURI("https://example.com/"), "install", pm.ALLOW_ACTION);

  const URL = "https://example.com/browser/browser/base/content/test/general/bug592338.html";
  BrowserTestUtils.openNewForegroundTab({ gBrowser, url: URL }).then(() => {
    let promise = promisePopupNotificationShown("addon-installed");
    BrowserTestUtils.synthesizeMouse("#theme-install", 2, 2, {}, gBrowser.selectedBrowser);
    promise.then(() => {
      is(LightweightThemeManager.currentTheme.id, "test", "Should have installed the test theme");

      LightweightThemeManager.currentTheme = null;
      gBrowser.removeTab(gBrowser.selectedTab);
      Services.perms.remove(makeURI("http://example.com/"), "install");

      runNextTest();
    });
  });
}
];

function runNextTest() {
  AddonManager.getAllInstalls(function(aInstalls) {
    is(aInstalls.length, 0, "Should be no active installs");

    if (TESTS.length == 0) {
      AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(aAddon) {
        aAddon.uninstall();

        Services.prefs.setBoolPref("extensions.logging.enabled", false);

        finish();
      });
      return;
    }

    info("Running " + TESTS[0].name);
    TESTS.shift()();
  });
}

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("extensions.logging.enabled", true);

  AddonManager.getInstallForURL(TESTROOT + "theme.xpi", function(aInstall) {
    aInstall.addListener({
      onInstallEnded() {
        AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(aAddon) {
          isnot(aAddon, null, "Should have installed the test theme.");

          runNextTest();
        });
      }
    });

    aInstall.install();
  }, "application/x-xpinstall");
}
