/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_BASE_HTTP = "http://example.com/browser/browser/devtools/commandline/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/browser/devtools/commandline/test/";

var { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
var { console } = require("resource://gre/modules/devtools/Console.jsm");
var DevToolsUtils = require("devtools/toolkit/DevToolsUtils");

// Import the GCLI test helper
var testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helpers.js", this);
Services.scriptloader.loadSubScript(testDir + "/mockCommands.js", this);

DevToolsUtils.testing = true;
SimpleTest.registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

/**
 * Force GC on shutdown, because it seems that GCLI can outrun the garbage
 * collector in some situations, which causes test failures in later tests
 * Bug 774619 is an example.
 */
registerCleanupFunction(function tearDown() {
  window.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .garbageCollect();
});
