/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_BASE = "chrome://mochitests/content/browser/browser/devtools/eyedropper/test/";
const TEST_HOST = 'mochi.test:8888';

const promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js").Promise;
const require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools.require;
const { Eyedropper } = require("devtools/eyedropper/eyedropper");


waitForExplicitFinish();

function cleanup()
{
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

registerCleanupFunction(cleanup);

function addTab(uri) {
  let deferred = promise.defer();

  let tab = gBrowser.addTab();

  gBrowser.selectedTab = tab;
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    deferred.resolve(tab);
  }, true);

  content.location = uri;

  return deferred.promise;
}
