/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let TargetFactory = gDevTools.TargetFactory;

const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

let TargetFactory = devtools.TargetFactory;

// All test are asynchronous
waitForExplicitFinish();

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
  Services.prefs.clearUserPref("devtools.dump.emit");
});

/**
 * Define an async test based on a generator function
 */
function asyncTest(generator) {
  return () => Task.spawn(generator).then(null, ok.bind(null, false)).then(finish);
}

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

function waitForTick() {
  let deferred = promise.defer();
  executeSoon(deferred.resolve);
  return deferred.promise;
}
