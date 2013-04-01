/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 777384 - Test mozapp permission.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function makeAllAppsLaunchable() {
  var Webapps = {};
  SpecialPowers.Cu.import("resource://gre/modules/Webapps.jsm", Webapps);
  var appRegistry = SpecialPowers.wrap(Webapps.DOMApplicationRegistry);

  var originalValue = appRegistry.allAppsLaunchable;
  appRegistry.allAppsLaunchable = true;

  // Clean up after ourselves once tests are done so the test page is unloaded.
  window.addEventListener("unload", function restoreAllAppsLaunchable(event) {
    if (event.target == window.document) {
      window.removeEventListener("unload", restoreAllAppsLaunchable, false);
      appRegistry.allAppsLaunchable = originalValue;
    }
  }, false);
}
makeAllAppsLaunchable();

function testAppElement(expectAnApp, callback) {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  iframe.setAttribute('mozapp', 'http://example.org/manifest.webapp');
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    is(e.detail.message == 'app', expectAnApp, e.detail.message);
    SimpleTest.executeSoon(callback);
  });
  document.body.appendChild(iframe);
  iframe.src = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_AppFramePermission.html';
}

function runTest() {
  SpecialPowers.addPermission("embed-apps", true, document);
  testAppElement(true, function() {
    SpecialPowers.removePermission("embed-apps", document);
    testAppElement(false, function() {
      SimpleTest.finish();
    });
  });
}

addEventListener('testready', runTest);
