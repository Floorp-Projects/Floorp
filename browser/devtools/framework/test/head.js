/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let TargetFactory = gDevTools.TargetFactory;

const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { ScratchpadManager } = Cu.import("resource:///modules/devtools/scratchpad-manager.jsm", {});

const URL_ROOT = "http://example.com/browser/browser/devtools/framework/test/";
const CHROME_URL_ROOT = "chrome://mochitests/content/browser/browser/devtools/framework/test/";

let TargetFactory = devtools.TargetFactory;

// All test are asynchronous
waitForExplicitFinish();

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

function getFrameScript() {
  let mm = gBrowser.selectedBrowser.messageManager;
  let frameURL = "chrome://browser/content/devtools/frame-script-utils.js";
  mm.loadFrameScript(frameURL, false);
  SimpleTest.registerCleanupFunction(() => {
    mm = null;
  });
  return mm;
}

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
  Services.prefs.clearUserPref("devtools.dump.emit");
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTab(url) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");
    def.resolve(tab);
  }, true);
  content.location = url;

  return def.promise;
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function synthesizeKeyFromKeyTag(aKeyId, document) {
  let key = document.getElementById(aKeyId);
  isnot(key, null, "Successfully retrieved the <key> node");

  let modifiersAttr = key.getAttribute("modifiers");

  let name = null;

  if (key.getAttribute("keycode"))
    name = key.getAttribute("keycode");
  else if (key.getAttribute("key"))
    name = key.getAttribute("key");

  isnot(name, null, "Successfully retrieved keycode/key");

  let modifiers = {
    shiftKey: modifiersAttr.match("shift"),
    ctrlKey: modifiersAttr.match("ctrl"),
    altKey: modifiersAttr.match("alt"),
    metaKey: modifiersAttr.match("meta"),
    accelKey: modifiersAttr.match("accel")
  };

  EventUtils.synthesizeKey(name, modifiers);
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        info("Got event: '" + eventName + "' on " + target + ".");
        target[remove](eventName, onEvent, useCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

/**
 * Some tests may need to import one or more of the test helper scripts.
 * A test helper script is simply a js file that contains common test code that
 * is either not common-enough to be in head.js, or that is located in a separate
 * directory.
 * The script will be loaded synchronously and in the test's scope.
 * @param {String} filePath The file path, relative to the current directory.
 *                 Examples:
 *                 - "helper_attributes_test_runner.js"
 *                 - "../../../commandline/test/helpers.js"
 */
function loadHelperScript(filePath) {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

function waitForTick() {
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}

function toggleAllTools(state) {
  for (let [, tool] of gDevTools._tools) {
    if (!tool.visibilityswitch) {
      continue;
    }
    if (state) {
      Services.prefs.setBoolPref(tool.visibilityswitch, true);
    } else {
      Services.prefs.clearUserPref(tool.visibilityswitch);
    }
  }
}

function getChromeActors(callback)
{
  let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
  let { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
  DebuggerServer.allowChromeProcess = true;

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  client.connect(() => {
    client.getProcess().then(response => {
      callback(client, response.form);
    });
  });

  SimpleTest.registerCleanupFunction(() => {
    DebuggerServer.destroy();
  });
}

function loadToolbox (url) {
  let { promise: p, resolve } = promise.defer();
  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target).then(resolve);
  }, true);

  content.location = url;
  return p;
}

function unloadToolbox (toolbox) {
  return toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();
  });
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}

/**
 * Open a Scratchpad window.
 *
 * @return nsIDOMWindow
 *         The new window object that holds Scratchpad.
 */
function *openScratchpadWindow () {
  let { promise: p, resolve } = promise.defer();
  let win = ScratchpadManager.openScratchpad();

  yield once(win, "load");

  win.Scratchpad.addObserver({
    onReady: function () {
      win.Scratchpad.removeObserver(this);
      resolve(win);
    }
  });
  return p;
}
