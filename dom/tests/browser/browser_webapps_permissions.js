/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEBUG = 0;
function log()
{
  if (DEBUG) {
    let output = [];
    for (let prop in arguments) {
      output.push(arguments[prop]);
    }
    dump("-*- browser_webapps_permissions test: " + output.join(" ") + "\n");
  }
}

let scope = {};
Cu.import("resource://gre/modules/PermissionSettings.jsm", scope);

const TEST_URL =
  "http://mochi.test:8888/browser/dom/tests/browser/test-webapps-permissions.html";
const TEST_MANIFEST_URL =
  "http://mochi.test:8888/browser/dom/tests/browser/test-webapp.webapp";
const TEST_ORIGIN_URL = "http://mochi.test:8888";

const installedPermsToTest = {
  "geolocation": "prompt",
  "alarms": "allow",
  "fmradio": "allow",
  "desktop-notification": "allow",
  "audio-channel-normal": "allow"
};

const uninstalledPermsToTest = {
  "geolocation": "unknown",
  "alarms": "unknown",
  "fmradio": "unknown",
  "desktop-notification": "unknown",
  "audio-channel-normal": "unknown"
};

var permManager = Cc["@mozilla.org/permissionmanager;1"]
                    .getService(Ci.nsIPermissionManager);
permManager.addFromPrincipal(window.document.nodePrincipal,
                             "webapps-manage",
                             Ci.nsIPermissionManager.ALLOW_ACTION);

var gWindow, gNavigator;

function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab(TEST_URL);
  gBrowser.selectedTab = tab;
  var browser = gBrowser.selectedBrowser;
  PopupNotifications.panel.addEventListener("popupshown", handlePopup, false);

  registerCleanupFunction(function () {
    gWindow = null;
    gBrowser.removeTab(tab);
  });

  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    gWindow = browser.contentWindow;

    SpecialPowers.setBoolPref("dom.mozPermissionSettings.enabled", true);
    SpecialPowers.addPermission("permissions", true, browser.contentWindow.document);
    SpecialPowers.addPermission("permissions", true, browser.contentDocument);

    executeSoon(function (){
      gWindow.focus();
      var nav = XPCNativeWrapper.unwrap(browser.contentWindow.navigator);
      ok(nav.mozApps, "we have a mozApps property");
      var navMozPerms = nav.mozPermissionSettings;
      ok(navMozPerms, "mozPermissions is available");

      // INSTALL app
      var pendingInstall = nav.mozApps.install(TEST_MANIFEST_URL, null);
      pendingInstall.onsuccess = function onsuccess()
      {
        ok(this.result, "we have a result: " + this.result);

        function testPerm(aPerm, aAccess)
        {
          var res =
            navMozPerms.get(aPerm, TEST_MANIFEST_URL, TEST_ORIGIN_URL, false);
          is(res, aAccess, "install: " + aPerm + " is " + res);
        }

        for (let permName in installedPermsToTest) {
          testPerm(permName, installedPermsToTest[permName]);
        }
        // uninstall checks
        uninstallApp();
      };

      pendingInstall.onerror = function onerror(e)
      {
        ok(false, "install()'s onerror was called: " + e);
        ok(false, "All permission checks failed, uninstal tests were not run");
      };
    });
  }, false);
}

function uninstallApp()
{
  var browser = gBrowser.selectedBrowser;
  var nav = XPCNativeWrapper.unwrap(browser.contentWindow.navigator);
  var navMozPerms = nav.mozPermissionSettings;

  var pending = nav.mozApps.getInstalled();
  pending.onsuccess = function onsuccess() {
    var m = this.result;
    for (var i = 0; i < m.length; i++) {
      var app = m[i];

      function uninstall() {
        var pendingUninstall = nav.mozApps.mgmt.uninstall(app);

        pendingUninstall.onsuccess = function(r) {
          // test to make sure all permissions have been removed
          function testPerm(aPerm, aAccess)
          {
            var res =
              navMozPerms.get(aPerm, TEST_MANIFEST_URL, TEST_ORIGIN_URL, false);
            is(res, aAccess, "uninstall: " + aPerm + " is " + res);
          }

          for (let permName in uninstalledPermsToTest) {
            testPerm(permName, uninstalledPermsToTest[permName]);
          }

          finish();
        };

        pending.onerror = function _onerror(e) {
          ok(false, e);
          ok(false, "All uninstall() permission checks failed!");

          finish();
        };
      };
      uninstall();
    }
  };
}

function handlePopup(aEvent)
{
  aEvent.target.removeEventListener("popupshown", handlePopup, false);
  SpecialPowers.wrap(this).childNodes[0].button.doCommand();
}
