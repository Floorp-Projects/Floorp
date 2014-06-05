/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_BASE_HTTP = "http://example.com/browser/browser/devtools/commandline/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/browser/devtools/commandline/test/";

var require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
var console = require("resource://gre/modules/devtools/Console.jsm").console;

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helpers.js", this);
Services.scriptloader.loadSubScript(testDir + "/mockCommands.js", this);

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
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
 * Creates a regular expression that matches a string. This greatly simplifies
 * matching and debugging long strings.
 *
 * @param {String} text
 *        Text to convert
 * @return {RegExp}
 *         Regular expression matching text
 */
function getRegexForString(str) {
  str = str.replace(/(\.|\\|\/|\(|\)|\[|\]|\*|\+|\?|\$|\^|\|)/g, "\\$1");
  return new RegExp(str);
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

function asyncTest(generator) {
  return () => Task.spawn(generator).then(null, ok.bind(null, false)).then(finish);
}
