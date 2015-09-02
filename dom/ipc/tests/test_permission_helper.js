"use strict";

// Used to access the DOM node in this test
const DOM_PARENT_ID = "container";
const DOM_PARENT = document.getElementById(DOM_PARENT_ID);
const APP_IFRAME_ID = "appFrame";
const APP_IFRAME_ID2 = "appFrame2";

// Settings for testing the permission
const SESSION_PERSIST_MINUTES = 10;
const PERMISSION_TYPE = "geolocation";
const permManager = SpecialPowers.Cc["@mozilla.org/permissionmanager;1"]
                    .getService(SpecialPowers.Ci.nsIPermissionManager);

// Used to access App's information like appId
const gAppsService = SpecialPowers.Cc["@mozilla.org/AppsService;1"]
                    .getService(SpecialPowers.Ci.nsIAppsService);

// Target-app for this testing
const APP_URL = "http://example.org";
const APP_MANIFEST = "http://example.org/manifest.webapp";

// Used to embed a remote app that open the test-target-app above
const embedAppHostedManifestURL = window.location.origin +
      '/tests/dom/ipc/tests/file_app.sjs?testToken=test_permission_embed.html&template=file_app.template.webapp';
var embedApp;

// Used to add listener for ipc:content-shutdown that
// will be triggered after ContentParent::DeallocateTabId
var gScript = SpecialPowers.loadChromeScript(SimpleTest.getTestFileURL('test_permission_framescript.js'));

// Delay reporting a result until after finish() got called
SimpleTest.waitForExplicitFinish();
// Allow tests to disable the per platform app validity checks
SpecialPowers.setAllAppsLaunchable(true);

// Run tests in order
function runTests() {
  if (!tests.length) {
    ok(true, "pass all tests!");
    SimpleTest.finish();
    return;
  }

  var test = tests.shift();
  test();
}

function test1() {
  allocateAppFrame(APP_IFRAME_ID, DOM_PARENT, APP_URL, APP_MANIFEST);

  var appId = gAppsService.getAppLocalIdByManifestURL(APP_MANIFEST);

  if (!SpecialPowers.hasPermission( PERMISSION_TYPE,
                                    { url: APP_URL,
                                      appId: appId,
                                      isInBrowserElement: false })) {
    errorHandler('[test 1] App should have permission: ' + PERMISSION_TYPE);
  }

  removeAppFrame(APP_IFRAME_ID);

  // We expect there is no permission for the test-target-app
  // after removing the in-process iframe embedding this app
  runNextIfAppHasPermission(1, false, APP_URL, APP_MANIFEST);
}

function test2() {
  var afterShutdown = function () {
    // We expect there is no permission for the test-target-app
    // after removing the remote iframe embedding this app
    runNextIfAppHasPermission(2, false, APP_URL, APP_MANIFEST);
  };

  // Test permission after the child-process containing
  // test-target-app is killed
  afterContentShutdown(1, afterShutdown);

  allocateAppFrame(APP_IFRAME_ID, DOM_PARENT, APP_URL, APP_MANIFEST, true);

  var appId = gAppsService.getAppLocalIdByManifestURL(APP_MANIFEST);

  if (!SpecialPowers.hasPermission( PERMISSION_TYPE,
                                    { url: APP_URL,
                                      appId: appId,
                                      isInBrowserElement: false })) {
    errorHandler('[test 2] App should have permission: ' + PERMISSION_TYPE);
  }

  removeAppFrame(APP_IFRAME_ID);
}

function test3() {
  var afterGrandchildShutdown = function () {
    // We expect there is no permission for the test-target-app
    // after removing the remote iframe embedding this app
    runNextIfAppHasPermission(3, false, APP_URL, APP_MANIFEST);
  };

  // Test permission after the grandchild-process
  // containing test-target-app is killed
  afterContentShutdown(1, afterGrandchildShutdown);

  allocateAppFrame(APP_IFRAME_ID,
                   DOM_PARENT,
                   embedApp.manifest.launch_path,
                   embedApp.manifestURL,
                   true);

  var appId = gAppsService.getAppLocalIdByManifestURL(APP_MANIFEST);

  if (!SpecialPowers.hasPermission(PERMISSION_TYPE,
                                   { url: APP_URL,
                                     appId: appId,
                                     isInBrowserElement: false })) {
    errorHandler('[test 3] App should have permission: ' + PERMISSION_TYPE);
  }
}

function test4() {
  var afterGrandchildShutdown = function () {
    // We expect there is still a permission for the test-target-app
    // after killing test-target-app on 3rd-level process
    runNextIfAppHasPermission(4, true, APP_URL, APP_MANIFEST);
  };

  // Test permission after the grandchild-process
  // containing test-target-app is killed
  afterContentShutdown(1, afterGrandchildShutdown);

  // Open a child process(2nd level) and a grandchild process(3rd level)
  // that contains a test-target-app
  allocateAppFrame(APP_IFRAME_ID,
                   DOM_PARENT,
                   embedApp.manifest.launch_path,
                   embedApp.manifestURL,
                   true);

  // Open another child process that contains
  // another test-target-app(2nd level)
  allocateAppFrame(APP_IFRAME_ID2,
                   DOM_PARENT,
                   APP_URL,
                   APP_MANIFEST,
                   true);

  var appId = gAppsService.getAppLocalIdByManifestURL(APP_MANIFEST);

  if (!SpecialPowers.hasPermission(PERMISSION_TYPE,
                                   { url: APP_URL,
                                     appId: appId,
                                     isInBrowserElement: false })) {
    errorHandler('[test 4] App should have permission: ' + PERMISSION_TYPE);
  }
}

function test5() {
  var afterShutdown = function () {
    // We expect there is no permission for the test-target-app
    // after crashing its parent-process
    runNextIfAppHasPermission(5, false, APP_URL, APP_MANIFEST);
  };

  // Test permission after the parent-process of test-target-app is crashed.
  afterContentShutdown(2, afterShutdown);

  allocateAppFrame(APP_IFRAME_ID,
                   DOM_PARENT,
                   embedApp.manifest.launch_path + '#' + encodeURIComponent('crash'),
                   embedApp.manifestURL,
                   true);

  var appId = gAppsService.getAppLocalIdByManifestURL(APP_MANIFEST);

  if (!SpecialPowers.hasPermission( PERMISSION_TYPE,
                                    { url: APP_URL,
                                      appId: appId,
                                      isInBrowserElement: false })) {
    errorHandler('[test 5] App should have permission: ' + PERMISSION_TYPE);
  }

  // Crash the child-process on 2nd level after
  // the grandchild process on 3rd is allocated
  var handler = {'crash': function() {
      gScript.sendAsyncMessage("crashreporter-status", {});

      getCrashReporterStatus(function(enable) {
        if (enable) {
          SimpleTest.expectChildProcessCrash();
        }
        crashChildProcess(APP_IFRAME_ID);
      });
    }
  };

  receiveMessageFromChildFrame(APP_IFRAME_ID, handler);
}

// Setup the prefrences and permissions.
// This function should be called before any tests
function setupPrefsAndPermissions() {
  SpecialPowers.pushPrefEnv({"set": [
    ["dom.mozBrowserFramesEnabled", true],
    ["dom.ipc.tabs.nested.enabled", true],
    ["dom.ipc.tabs.disabled", false]]},
    function() {
      var autoManageApp = function () {
        SpecialPowers.setAllAppsLaunchable(true);
        // No confirmation needed when an app is installed and uninstalled.
        SpecialPowers.autoConfirmAppInstall(() => {
          // This callback should trigger the first test
          SpecialPowers.autoConfirmAppUninstall(runTests);
        });
      };

      setupOpenAppPermission(document, autoManageApp);
    }
  );
}

function setupOpenAppPermission(ctx, callback) {
  SpecialPowers.pushPermissions([
    { "type": "browser", "allow": true, "context": ctx}, // for mozbrowser
    { "type": "embed-apps", "allow": true, "context": ctx }, // for mozapp
    { "type": "webapps-manage", "allow": true, "context": ctx }], // for (un)installing apps
    function checkPermissions() {
      if (SpecialPowers.hasPermission("browser", ctx) &&
          SpecialPowers.hasPermission("embed-apps", ctx) &&
          SpecialPowers.hasPermission("webapps-manage", ctx)) {
        callback();
      } else {
        errorHandler(">> At least one of required permissions to open app is disallowed!\n");
      }
    });
}

function installApp() {
  // Install App from manifest
  var request = navigator.mozApps.install(embedAppHostedManifestURL);
  request.onerror = cbError;
  request.onsuccess = function() {
    // Get installed app
    embedApp = request.result; // Assign to global variable
    runTests();
  }
}

function uninstallApp() {
  var request = navigator.mozApps.mgmt.uninstall(embedApp);
  request.onerror = cbError;
  request.onsuccess = function() {
    info("uninstall app susseccfully!");
    runTests();
  }
}

function removeAppFrame(id) {
  var ifr  = document.getElementById(id);
  ifr.remove();
}

function allocateAppFrame(id, parentNode, appSRC, manifestURL, useRemote = false) {
  var ifr = document.createElement('iframe');
  ifr.setAttribute('id', id);
  ifr.setAttribute('remote', useRemote? "true" : "false");
  ifr.setAttribute('mozbrowser', 'true');
  ifr.setAttribute('mozapp', manifestURL);
  ifr.setAttribute('src', appSRC);
  parentNode.appendChild(ifr);
}

function receiveMessageFromChildFrame(id, handlers) {
  var ifr = document.getElementById(id);
  ifr.addEventListener('mozbrowsershowmodalprompt', function (recvMsg) {
    var msg = recvMsg.detail.message;
    handlers[msg]();
  });
}

function addPermissionToApp(appURL, manifestURL) {
  var appId = gAppsService.getAppLocalIdByManifestURL(manifestURL);

  // Get the time now. This is used for permission manager's expire_time
  var now = Number(Date.now());

  // Add app's permission asynchronously
  SpecialPowers.pushPermissions([
      { "type":PERMISSION_TYPE,
        "allow": 1,
        "expireType":permManager.EXPIRE_SESSION,
        "expireTime":now + SESSION_PERSIST_MINUTES*60*1000,
        "context": { url: appURL,
                     appId: appId,
                     isInBrowserElement:false }
      }
    ], function() {
      runTests();
    });
}

function runNextIfAppHasPermission(round, expect, appURL, manifestURL) {
  var appId = gAppsService.getAppLocalIdByManifestURL(manifestURL);

  var hasPerm = SpecialPowers.hasPermission(PERMISSION_TYPE,
                                            { url: appURL,
                                              appId: appId,
                                              isInBrowserElement: false });
  var result = (expect==hasPerm);
  if (result) {
    runTests();
  } else {
    errorHandler( '[test ' + round + '] App should ' + ((expect)? '':'NOT ') +
                  'have permission: ' + PERMISSION_TYPE);
  }
}

function afterContentShutdown(times, callback) {
  // handle the message from test_permission_framescript.js
  var num = 0;
  gScript.addMessageListener('content-shutdown', function onShutdown(data) {
    num += 1;
    if (num >= times) {
      gScript.removeMessageListener('content-shutdown', onShutdown);
      callback();
    }
  });
}

function getCrashReporterStatus(callback) {
  gScript.addMessageListener('crashreporter-enable', function getStatus(data) {
    gScript.removeMessageListener('crashreporter-enable', getStatus);
    callback(data);
  });
}

// Inject a frame script that crashes the content process
function crashChildProcess(frameId) {
  var child = document.getElementById(frameId);
  var mm = SpecialPowers.getBrowserFrameMessageManager(child);
  var childFrameScriptStr =
    'function ContentScriptScope() {' +
      'var Cu = Components.utils;' +
      'Cu.import("resource://gre/modules/ctypes.jsm");' +
      'var crash = function() {' +
        'var zero = new ctypes.intptr_t(8);' +
        'var badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));' +
        'badptr.contents;' +
      '};' +
      'privateNoteIntentionalCrash();' +
      'crash();' +
    '}';
  mm.loadFrameScript('data:,new ' + childFrameScriptStr, false);
}

function errorHandler(errMsg) {
  ok(false, errMsg);
  SimpleTest.finish();
}

function cbError(e) {
  errorHandler("Error callback invoked: " + this.error.name);
}
