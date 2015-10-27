"use strict";

var gAppsService = SpecialPowers.Cc["@mozilla.org/AppsService;1"]
                   .getService(SpecialPowers.Ci.nsIAppsService);


var gApp;

function cbError(e) {
  ok(false, "Error callback invoked: " + this.error.name);
  SimpleTest.finish();
}

function installApp(aTestToken, aTemplate) {
  var hostedManifestURL = window.location.origin +
                          '/tests/dom/tv/test/mochitest/file_app.sjs?testToken='
                          + aTestToken + '&template=' + aTemplate;
  var request = navigator.mozApps.install(hostedManifestURL);
  request.onerror = cbError;
  request.onsuccess = function() {
    gApp = request.result;

    var appId = gAppsService.getAppLocalIdByManifestURL(gApp.manifestURL);
    SpecialPowers.addPermission("tv", true, { url: gApp.origin,
                                              appId: appId,
                                              isInBrowserElement: false });

    runTest();
  }
}

function uninstallApp() {
  var request = navigator.mozApps.mgmt.uninstall(gApp);
  request.onerror = cbError;
  request.onsuccess = function() {
    // All done.
    info("All done");
    runTest();
  }
}

function testApp() {
  var ifr = document.createElement('iframe');
  ifr.setAttribute('mozbrowser', 'true');
  ifr.setAttribute('mozapp', gApp.manifestURL);
  ifr.setAttribute('src', gApp.manifest.launch_path);
  var domParent = document.getElementById('content');

  // Set us up to listen for messages from the app.
  var listener = function(e) {
    var message = e.detail.message;
    if (/^OK/.exec(message)) {
      ok(true, "Message from app: " + message);
    } else if (/KO/.exec(message)) {
      ok(false, "Message from app: " + message);
    } else if (/^INFO/.exec(message)) {
      info("Message from app: " + message);
    } else if (/DONE/.exec(message)) {
      ok(true, "Messaging from app complete");
      ifr.removeEventListener('mozbrowsershowmodalprompt', listener);
      domParent.removeChild(ifr);
      runTest();
    }
  }

  // This event is triggered when the app calls "alert".
  ifr.addEventListener('mozbrowsershowmodalprompt', listener, false);
  domParent.appendChild(ifr);
}

function runTest() {
  if (!tests.length) {
    SimpleTest.finish();
    return;
  }

  var test = tests.shift();
  test();
}

function setupPrefsAndPermissions() {
  SpecialPowers.pushPrefEnv({"set": [["dom.tv.enabled", true],
                                     ["dom.testing.tv_enabled_for_hosted_apps", true]]}, function() {
    SpecialPowers.pushPermissions(
      [{ "type": "browser", "allow": true, "context": document },
       { "type": "embed-apps", "allow": true, "context": document },
       { "type": "webapps-manage", "allow": true, "context": document }],
      function() {
        SpecialPowers.setAllAppsLaunchable(true);
        SpecialPowers.setBoolPref("dom.mozBrowserFramesEnabled", true);
        // No confirmation needed when an app is installed and uninstalled.
        SpecialPowers.autoConfirmAppInstall(() => {
          SpecialPowers.autoConfirmAppUninstall(runTest);
        });
      });
  });
}
