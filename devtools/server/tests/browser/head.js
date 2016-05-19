/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

const {console} = Cu.import("resource://gre/modules/Console.jsm", {});
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {DebuggerClient} = require("devtools/shared/client/main");
const {DebuggerServer} = require("devtools/server/main");
const {defer} = require("promise");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const Services = require("Services");

const PATH = "browser/devtools/server/tests/browser/";
const MAIN_DOMAIN = "http://test1.example.org/" + PATH;
const ALT_DOMAIN = "http://sectest1.example.org/" + PATH;
const ALT_DOMAIN_SECURED = "https://sectest1.example.org:443/" + PATH;

// All tests are asynchronous.
waitForExplicitFinish();

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the new browser that the document
 *         is loaded in. Note that we cannot return the document
 *         directly, since this would be a CPOW in the e10s case,
 *         and Promises cannot be resolved with CPOWs (see bug 1233497).
 */
var addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let loaded = once(gBrowser.selectedBrowser, "load", true);

  content.location = url;
  yield loaded;

  info("URL '" + url + "' loading complete");

  yield new Promise(resolve => {
    let isBlank = url == "about:blank";
    waitForFocus(resolve, content, isBlank);
  });

  return tab.linkedBrowser;
});

function* initAnimationsFrontForUrl(url) {
  const {AnimationsFront} = require("devtools/server/actors/animation");
  const {InspectorFront} = require("devtools/shared/fronts/inspector");

  yield addTab(url);

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let inspector = InspectorFront(client, form);
  let walker = yield inspector.getWalker();
  let animations = AnimationsFront(client, form);

  return {inspector, walker, animations, client};
}

function initDebuggerServer() {
  try {
    // Sometimes debugger server does not get destroyed correctly by previous
    // tests.
    DebuggerServer.destroy();
  } catch (ex) { }
  DebuggerServer.init();
  DebuggerServer.addBrowserActors();
}

/**
 * Connect a debugger client.
 * @param {DebuggerClient}
 * @return {Promise} Resolves to the selected tabActor form when the client is
 * connected.
 */
function connectDebuggerClient(client) {
  return client.connect()
    .then(() => client.listTabs())
    .then(tabs => {
      return tabs.tabs[tabs.selected];
    });
}

/**
 * Close a debugger client's connection.
 * @param {DebuggerClient}
 * @return {Promise} Resolves when the connection is closed.
 */
function closeDebuggerClient(client) {
  return new Promise(resolve => client.close(resolve));
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture = false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  return new Promise(resolve => {

    for (let [add, remove] of [
      ["addEventListener", "removeEventListener"],
      ["addListener", "removeListener"],
      ["on", "off"]
    ]) {
      if ((add in target) && (remove in target)) {
        target[add](eventName, function onEvent(...aArgs) {
          info("Got event: '" + eventName + "' on " + target + ".");
          target[remove](eventName, onEvent, useCapture);
          resolve(...aArgs);
        }, useCapture);
        break;
      }
    }
  });
}

/**
 * Forces GC, CC and Shrinking GC to get rid of disconnected docshells and
 * windows.
 */
function forceCollections() {
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();
}

/**
 * Get a mock tabActor from a given window.
 * This is sometimes useful to test actors or classes that use the tabActor in
 * isolation.
 * @param {DOMWindow} win
 * @return {Object}
 */
function getMockTabActor(win) {
  return {
    window: win,
    isRootActor: true
  };
}

registerCleanupFunction(function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

function busyWait(time) {
  let start = Date.now();
  let stack;
  while (Date.now() - start < time) { stack = Components.stack; }
}

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
function waitUntil(predicate, interval = 10) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function () {
      waitUntil(predicate).then(() => resolve(true));
    }, interval);
  });
}

function waitForMarkerType(front, types, predicate,
  unpackFun = (name, data) => data.markers,
  eventName = "timeline-data")
{
  types = [].concat(types);
  predicate = predicate || function () { return true; };
  let filteredMarkers = [];
  let { promise, resolve } = defer();

  info("Waiting for markers of type: " + types);

  function handler(name, data) {
    if (typeof name === "string" && name !== "markers") {
      return;
    }

    let markers = unpackFun(name, data);
    info("Got markers: " + JSON.stringify(markers, null, 2));

    filteredMarkers = filteredMarkers.concat(markers.filter(m => types.indexOf(m.name) !== -1));

    if (types.every(t => filteredMarkers.some(m => m.name === t)) && predicate(filteredMarkers)) {
      front.off(eventName, handler);
      resolve(filteredMarkers);
    }
  }
  front.on(eventName, handler);

  return promise;
}
