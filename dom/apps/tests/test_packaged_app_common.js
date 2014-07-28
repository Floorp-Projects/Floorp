/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var PackagedTestHelper = (function PackagedTestHelper() {
  "use strict";

  var steps;
  var index = -1;
  var gSJSPath = "tests/dom/apps/tests/file_packaged_app.sjs";
  var gSJS = "http://test/" + gSJSPath;
  var gAppName = "appname";
  var gApp = null;
  var gInstallOrigin = "http://mochi.test:8888";

  function debug(aMsg) {
    //dump("== PackageTestHelper debug == " + aMsg + "\n");
  }

  function next() {
    index += 1;
    if (index >= steps.length) {
      ok(false, "Shouldn't get here!");
      return;
    }
    try {
      steps[index]();
    } catch(ex) {
      ok(false, "Caught exception", ex);
    }
  }

  function start() {
    next();
  }

  function finish() {
    SpecialPowers.removePermission("webapps-manage", document);
    SpecialPowers.removePermission("browser", document);
    SimpleTest.finish();
  }

  function mozAppsError() {
    ok(false, "mozApps error: " + this.error.name);
    finish();
  }

  function xhrError(event, url) {
    var xhr = event.target;
    ok(false, "XHR error loading " + url + ": " + xhr.status + " - " +
       xhr.statusText);
    finish();
  }

  function xhrAbort(url) {
    ok(false, "XHR abort loading " + url);
    finish();
  }

  function setAppVersion(aVersion, aCb, aDontUpdatePackage, aAllowCancel) {
    var xhr = new XMLHttpRequest();
    var dontUpdate = "";
    var allowCancel = "";
    if (aDontUpdatePackage) {
      dontUpdate = "&dontUpdatePackage=1";
    }
    if (aAllowCancel) {
      allowCancel= "&allowCancel=1";
    }
    var url = gSJS + "?setVersion=" + aVersion + dontUpdate + allowCancel;
    xhr.addEventListener("load", function() {
                           is(xhr.responseText, "OK", "setAppVersion OK");
                           aCb();
                         });
    xhr.addEventListener("error", event => xhrError(event, url));
    xhr.addEventListener("abort", event => xhrAbort(url));
    xhr.open("GET", url, true);
    xhr.send();
  }

  function checkAppDownloadError(aMiniManifestURL,
                                 aExpectedError,
                                 aVersion,
                                 aUninstall,
                                 aDownloadAvailable,
                                 aName,
                                 aCb) {
    var req = navigator.mozApps.installPackage(aMiniManifestURL);
    req.onsuccess = function(evt) {
      ok(true, "App installed");
      if (!aUninstall) {
        // Save it for later.
        gApp = req.result;
      }
    };
    req.onerror = function(evt) {
      ok(false, "Got unexpected " + evt.target.error.name);
      finish();
    };

    navigator.mozApps.mgmt.oninstall = function(evt) {
      var aApp = evt.application;
      aApp.ondownloaderror = function(evt) {
        var error = aApp.downloadError.name;
        if (error == aExpectedError) {
          ok(true, "Got expected " + aExpectedError);
          var expected = {
            name: aName,
            manifestURL: aMiniManifestURL,
            installOrigin: gInstallOrigin,
            progress: 0,
            installState: "pending",
            downloadAvailable: aDownloadAvailable,
            downloading: false,
            downloadSize: 0,
            size: 0,
            readyToApplyDownload: false
          };
          checkAppState(aApp, aVersion, expected, false, aUninstall,
                        aCb || next);
        } else {
          ok(false, "Got unexpected " + error);
          finish();
        }
      };
      aApp.ondownloadsuccess = function(evt) {
        ok(false, "We are supposed to throw " + aExpectedError);
        finish();
      };
    };
  }

  function checkAppState(aApp,
                         aVersion,
                         aExpectedApp,
                         aLaunchable,
                         aUninstall,
                         aCb) {
    debug(JSON.stringify(aApp, null, 2));
    if (aApp.manifest) {
      debug(JSON.stringify(aApp.manifest, null, 2));
    }

    if (aExpectedApp.name) {
      if (aApp.manifest) {
        is(aApp.manifest.name, aExpectedApp.name, "Check name");
      }
      is(aApp.updateManifest.name, aExpectedApp.name, "Check name mini-manifest");
    }
    if (aApp.manifest) {
      is(aApp.manifest.version, aVersion, "Check version");
    }
    if (typeof aExpectedApp.size !== "undefined" && aApp.manifest) {
      is(aApp.manifest.size, aExpectedApp.size, "Check size");
    }
    if (aApp.manifest) {
      is(aApp.manifest.launch_path, aExpectedApp.launch_path || gSJSPath, "Check launch path");
    }
    if (aExpectedApp.manifestURL) {
      is(aApp.manifestURL, aExpectedApp.manifestURL, "Check manifestURL");
    }
    if (aExpectedApp.installOrigin) {
      is(aApp.installOrigin, aExpectedApp.installOrigin, "Check installOrigin");
    }
    ok(aApp.removable, "Removable app");
    if (typeof aExpectedApp.progress !== "undefined") {
      todo(aApp.progress == aExpectedApp.progress, "Check progress");
    }
    if (aExpectedApp.installState) {
      is(aApp.installState, aExpectedApp.installState, "Check installState");
    }
    if (typeof aExpectedApp.downloadAvailable !== "undefined") {
      is(aApp.downloadAvailable, aExpectedApp.downloadAvailable,
         "Check download available");
    }
    if (typeof aExpectedApp.downloading !== "undefined") {
      is(aApp.downloading, aExpectedApp.downloading, "Check downloading");
    }
    if (typeof aExpectedApp.downloadSize !== "undefined") {
      is(aApp.downloadSize, aExpectedApp.downloadSize, "Check downloadSize");
    }
    if (typeof aExpectedApp.readyToApplyDownload !== "undefined") {
      is(aApp.readyToApplyDownload, aExpectedApp.readyToApplyDownload,
         "Check readyToApplyDownload");
    }
    if (typeof aExpectedApp.origin !== "undefined") {
      is(aApp.origin, aExpectedApp.origin, "Check origin");
    }
    if (aLaunchable) {
      if (aUninstall) {
        checkUninstallApp(aApp);
      } else if (aCb && typeof aCb === "function") {
        aCb();
      }
      return;
    }

    // Check if app is not launchable.
    var req = aApp.launch();
    req.onsuccess = function () {
      ok(false, "We shouldn't be here");
      finish();
    };
    req.onerror = function() {
      ok(true, "App is not launchable");
      if (aUninstall) {
        checkUninstallApp(aApp);
      } else if (aCb && typeof aCb === "function") {
        aCb();
      }
      return;
    };
  }

  return {
    setSteps: function (aSteps) {
      steps = aSteps;
    },
    next: next,
    start: start,
    finish: finish,
    mozAppsError: mozAppsError,
    setAppVersion: setAppVersion,
    checkAppState: checkAppState,
    checkAppDownloadError: checkAppDownloadError,
    get gSJSPath() { return gSJSPath; },
    set gSJSPath(aValue) { gSJSPath = aValue },
    get gSJS() { return gSJS; },
    get gAppName() { return gAppName;},
    get gApp() { return gApp; },
    set gApp(aValue) { gApp = aValue; },
    gInstallOrigin: gInstallOrigin
  };

})();
