/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";

var tempScope = {};
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
var LightweightThemeManager = tempScope.LightweightThemeManager;

function wait_for_notification(aCallback) {
  PopupNotifications.panel.addEventListener("popupshown", function() {
    aCallback(PopupNotifications.panel);
  }, {once: true});
}

var TESTS = [
function test_install_http() {
  is(LightweightThemeManager.currentTheme, null, "Should be no lightweight theme selected");

  var pm = Services.perms;
  pm.add(makeURI("http://example.org/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = gBrowser.addTab("http://example.org/browser/browser/base/content/test/general/bug592338.html");
  gBrowser.selectedBrowser.addEventListener("pageshow", function() {
    if (gBrowser.contentDocument.location.href == "about:blank")
      return;

    gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee);

    executeSoon(function() {
      BrowserTestUtils.synthesizeMouse("#theme-install", 2, 2, {}, gBrowser.selectedBrowser);

      is(LightweightThemeManager.currentTheme, null, "Should not have installed the test theme");

      gBrowser.removeTab(gBrowser.selectedTab);

      pm.remove(makeURI("http://example.org/"), "install");

      runNextTest();
    });
  });
},

function test_install_lwtheme() {
  is(LightweightThemeManager.currentTheme, null, "Should be no lightweight theme selected");

  var pm = Services.perms;
  pm.add(makeURI("https://example.com/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = gBrowser.addTab("https://example.com/browser/browser/base/content/test/general/bug592338.html");
  gBrowser.selectedBrowser.addEventListener("pageshow", function() {
    if (gBrowser.contentDocument.location.href == "about:blank")
      return;

    gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee);

    BrowserTestUtils.synthesizeMouse("#theme-install", 2, 2, {}, gBrowser.selectedBrowser);
    let notificationBox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
    waitForCondition(
      () => notificationBox.getNotificationWithValue("lwtheme-install-notification"),
      () => {
        is(LightweightThemeManager.currentTheme.id, "test", "Should have installed the test theme");

        LightweightThemeManager.currentTheme = null;
        gBrowser.removeTab(gBrowser.selectedTab);
        Services.perms.remove(makeURI("http://example.com/"), "install");

        runNextTest();
      }
    );
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
