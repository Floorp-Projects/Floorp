/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_BASE = "chrome://mochitests/content/browser/browser/devtools/eyedropper/test/";
const TEST_HOST = 'mochi.test:8888';

let { devtools } = Components.utils.import("resource://gre/modules/devtools/Loader.jsm", {});
const { Eyedropper, EyedropperManager } = devtools.require("devtools/eyedropper/eyedropper");
const { Promise: promise } = devtools.require("resource://gre/modules/Promise.jsm");

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "../../../commandline/test/helpers.js", this);

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

function dropperLoaded(dropper) {
  if (dropper.loaded) {
    return promise.resolve();
  }
  return dropper.once("load");
}
