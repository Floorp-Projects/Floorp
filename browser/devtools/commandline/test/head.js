/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_BASE_HTTP = "http://example.com/browser/browser/devtools/commandline/test/";
const TEST_BASE_HTTPS = "https://example.com/browser/browser/devtools/commandline/test/";

let console = (function() {
  let tempScope = {};
  Components.utils.import("resource://gre/modules/devtools/Console.jsm", tempScope);
  return tempScope.console;
})();

let TargetFactory = (function() {
  let tempScope = {};
  Components.utils.import("resource:///modules/devtools/Target.jsm", tempScope);
  return tempScope.TargetFactory;
})();

let Promise = (function() {
  let tempScope = {};
  Components.utils.import("resource://gre/modules/commonjs/promise/core.js", tempScope);
  return tempScope.Promise;
})();

// Import the GCLI test helper
let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));

Services.scriptloader.loadSubScript(testDir + "/helpers.js", this);
Services.scriptloader.loadSubScript(testDir + "/mockCommands.js", this);
Services.scriptloader.loadSubScript(testDir + "/helpers_perwindowpb.js", this);

/**
 * Open a new tab at a URL and call a callback on load
 */
function addTab(aURL, aCallback)
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  if (aURL != null) {
    content.location = aURL;
  }

  let deferred = Promise.defer();

  let tab = gBrowser.selectedTab;
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let browser = gBrowser.getBrowserForTab(tab);

  function onTabLoad() {
    browser.removeEventListener("load", onTabLoad, true);

    if (aCallback != null) {
      aCallback(browser, tab, browser.contentDocument);
    }

    deferred.resolve({ browser: browser, tab: tab, target: target });
  }

  browser.addEventListener("load", onTabLoad, true);
  return deferred.promise;
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }

  console = undefined;
});
