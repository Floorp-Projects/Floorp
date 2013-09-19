/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TESTROOT = "http://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";

var tempScope = {};
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", tempScope);
var LightweightThemeManager = tempScope.LightweightThemeManager;

function wait_for_notification(aCallback) {
  PopupNotifications.panel.addEventListener("popupshown", function() {
    PopupNotifications.panel.removeEventListener("popupshown", arguments.callee, false);
    aCallback(PopupNotifications.panel);
  }, false);
}

var TESTS = [
function test_install_lwtheme() {
  is(LightweightThemeManager.currentTheme, null, "Should be no lightweight theme selected");

  var pm = Services.perms;
  pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

  gBrowser.selectedTab = gBrowser.addTab("http://example.com/browser/browser/base/content/test/general/bug592338.html");
  gBrowser.selectedBrowser.addEventListener("pageshow", function() {
    if (gBrowser.contentDocument.location.href == "about:blank")
      return;

    gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee, false);

    executeSoon(function() {
      var link = gBrowser.contentDocument.getElementById("theme-install");
      EventUtils.synthesizeMouse(link, 2, 2, {}, gBrowser.contentWindow);

      is(LightweightThemeManager.currentTheme.id, "test", "Should have installed the test theme");

      LightweightThemeManager.currentTheme = null;
      gBrowser.removeTab(gBrowser.selectedTab);

      Services.perms.remove("example.com", "install");

      runNextTest();
    });
  }, false);
},

function test_lwtheme_switch_theme() {
  is(LightweightThemeManager.currentTheme, null, "Should be no lightweight theme selected");

  AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(aAddon) {
    aAddon.userDisabled = false;
    ok(aAddon.isActive, "Theme should have immediately enabled");
    Services.prefs.setBoolPref("extensions.dss.enabled", false);

    var pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    gBrowser.selectedTab = gBrowser.addTab("http://example.com/browser/browser/base/content/test/general/bug592338.html");
    gBrowser.selectedBrowser.addEventListener("pageshow", function() {
      if (gBrowser.contentDocument.location.href == "about:blank")
        return;

      gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee, false);

      executeSoon(function() {
        var link = gBrowser.contentDocument.getElementById("theme-install");
        wait_for_notification(function(aPanel) {
          is(LightweightThemeManager.currentTheme, null, "Should not have installed the test lwtheme");
          ok(aAddon.isActive, "Test theme should still be active");

          let notification = aPanel.childNodes[0];
          is(notification.button.label, "Restart Now", "Should have seen the right button");

          ok(aAddon.userDisabled, "Should be waiting to disable the test theme");
          aAddon.userDisabled = false;
          Services.prefs.setBoolPref("extensions.dss.enabled", true);

          gBrowser.removeTab(gBrowser.selectedTab);

          Services.perms.remove("example.com", "install");

          runNextTest();
        });
        EventUtils.synthesizeMouse(link, 2, 2, {}, gBrowser.contentWindow);
      });
    }, false);
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
        Services.prefs.setBoolPref("extensions.dss.enabled", false);

        finish();
      });
      return;
    }

    info("Running " + TESTS[0].name);
    TESTS.shift()();
  });
};

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref("extensions.logging.enabled", true);

  AddonManager.getInstallForURL(TESTROOT + "theme.xpi", function(aInstall) {
    aInstall.addListener({
      onInstallEnded: function(aInstall, aAddon) {
        AddonManager.getAddonByID("theme-xpi@tests.mozilla.org", function(aAddon) {
          isnot(aAddon, null, "Should have installed the test theme.");

          // In order to switch themes while the test is running we turn on dynamic
          // theme switching. This means the test isn't exactly correct but should
          // do some good
          Services.prefs.setBoolPref("extensions.dss.enabled", true);

          runNextTest();
        });
      }
    });

    aInstall.install();
  }, "application/x-xpinstall");
}
