/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  "contacts": "deny",
  "device-storage:apps": "deny",
};

const reinstalledPermsToTest = {
  "geolocation": "prompt",
  "alarms": "unknown",
  "contacts": "deny",
  "device-storage:apps": "deny",
};

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

    let pendingInstall;

    function testInstall() {
      var nav = XPCNativeWrapper.unwrap(browser.contentWindow.navigator);
      ok(nav.mozApps, "we have a mozApps property");
      var navMozPerms = nav.mozPermissionSettings;
      ok(navMozPerms, "mozPermissions is available");

      // INSTALL app
      pendingInstall = nav.mozApps.install(TEST_MANIFEST_URL, null);
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

        writeUpdatesToWebappManifest();
      };

      pendingInstall.onerror = function onerror(e)
      {
        ok(false, "install()'s onerror was called: " + e);
        ok(false, "All permission checks failed, reinstall tests were not run");
      };
    }
    testInstall();
  }, false);
}

function reinstallApp()
{
  var browser = gBrowser.selectedBrowser;
  var nav = XPCNativeWrapper.unwrap(browser.contentWindow.navigator);
  var navMozPerms = nav.mozPermissionSettings;

  var pendingReinstall = nav.mozApps.install(TEST_MANIFEST_URL);
  pendingReinstall.onsuccess = function onsuccess()
  {
    ok(this.result, "we have a result: " + this.result);

    function testPerm(aPerm, aAccess)
    {
      var res =
        navMozPerms.get(aPerm, TEST_MANIFEST_URL, TEST_ORIGIN_URL, false);
        is(res, aAccess, "reinstall: " + aPerm + " is " + res);
      }

      for (let permName in reinstalledPermsToTest) {
        testPerm(permName, reinstalledPermsToTest[permName]);
      }
      writeUpdatesToWebappManifest();
      finish();
  };
};

var qtyPopups = 0;

function handlePopup(aEvent)
{
  qtyPopups++;
  if (qtyPopups == 2) {
    aEvent.target.removeEventListener("popupshown", handlePopup, false);
  }
  SpecialPowers.wrap(this).childNodes[0].button.doCommand();
}

function writeUpdatesToWebappManifest(aRestore)
{
  let newfile = Cc["@mozilla.org/file/directory_service;1"].
                  getService(Ci.nsIProperties).
                  get("XCurProcD", Ci.nsIFile);

  let devPath = ["_tests", "testing", "mochitest",
                 "browser", "dom" , "tests", "browser"];
  // make package-tests moves tests to:
  // dist/test-stage/mochitest/browser/dom/tests/browser
  let slavePath = ["dist", "test-stage", "mochitest",
                   "browser", "dom", "tests", "browser"];

  newfile = newfile.parent; // up to dist/
  newfile = newfile.parent;// up to obj-dir/

  info(newfile.path);
  try {
    for (let idx in devPath) {
      newfile.append(devPath[idx]);
      if (!newfile.isDirectory()) {
        info("*** NOT RUNNING ON DEV MACHINE\n\n");
        throw new Error("Test is not running on dev machine, try dev path");
      }
    }
  }
  catch (ex) {
    info(ex + "\n\n");
    for (let idx in slavePath) {
      newfile.append(slavePath[idx]);
      if (!newfile.isDirectory()) {
        info("*** NOT RUNNING ON BUILD SLAVE\n\n");
        throw new Error("Test is not running on slave machine, abort test");
      }
    }
  }

  info("Final path: " + newfile.path);

  if (aRestore) {
    newfile.append("test-webapp-original.webapp");
  } else {
    newfile.append("test-webapp-reinstall.webapp");
  }

  let oldfile = newfile.parent;
  oldfile.append("test-webapp.webapp");

  newfile.copyTo(null, "test-webapp.webapp");

  if (!aRestore) {
    executeSoon(function (){ reinstallApp(); });
  }
}
