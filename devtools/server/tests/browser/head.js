/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../client/shared/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

const {DebuggerClient} = require("devtools/shared/client/debugger-client");
const {DebuggerServer} = require("devtools/server/main");

const PATH = "browser/devtools/server/tests/browser/";
const MAIN_DOMAIN = "http://test1.example.org/" + PATH;
const ALT_DOMAIN = "http://sectest1.example.org/" + PATH;
const ALT_DOMAIN_SECURED = "https://sectest1.example.org:443/" + PATH;

// GUID to be used as a separator in compound keys. This must match the same
// constant in devtools/server/actors/storage.js,
// devtools/client/storage/ui.js and devtools/client/storage/test/head.js
const SEPARATOR_GUID = "{9d414cc5-8319-0a04-0586-c0a6ae01670a}";

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
var addTab = async function(url) {
  info(`Adding a new tab with URL: ${url}`);
  const tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  info(`Tab added and URL ${url} loaded`);

  return tab.linkedBrowser;
};

async function initAnimationsFrontForUrl(url) {
  const {AnimationsFront} = require("devtools/shared/fronts/animation");
  const {InspectorFront} = require("devtools/shared/fronts/inspector");

  await addTab(url);

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const inspector = InspectorFront(client, form);
  const walker = await inspector.getWalker();
  const animations = AnimationsFront(client, form);

  return {inspector, walker, animations, client};
}

async function initLayoutFrontForUrl(url) {
  const {InspectorFront} = require("devtools/shared/fronts/inspector");

  await addTab(url);

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const inspector = InspectorFront(client, form);
  const walker = await inspector.getWalker();
  const layout = await walker.getLayoutInspector();

  return {inspector, walker, layout, client};
}

async function initAccessibilityFrontForUrl(url) {
  const {AccessibilityFront} = require("devtools/shared/fronts/accessibility");
  const {InspectorFront} = require("devtools/shared/fronts/inspector");

  await addTab(url);

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);
  const inspector = InspectorFront(client, form);
  const walker = await inspector.getWalker();
  const accessibility = AccessibilityFront(client, form);

  await accessibility.bootstrap();

  return {inspector, walker, accessibility, client};
}

function initDebuggerServer() {
  try {
    // Sometimes debugger server does not get destroyed correctly by previous
    // tests.
    DebuggerServer.destroy();
  } catch (e) {
    info(`DebuggerServer destroy error: ${e}\n${e.stack}`);
  }
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
}

async function initPerfFront() {
  const {PerfFront} = require("devtools/shared/fronts/perf");

  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await waitUntilClientConnected(client);
  const rootForm = await getRootForm(client);
  const front = PerfFront(client, rootForm);
  return {front, client};
}

/**
 * Gets the RootActor form from a DebuggerClient.
 * @param {DebuggerClient} client
 * @return {RootActor} Resolves when connected.
 */
function getRootForm(client) {
  return client.listTabs();
}

/**
 * Wait until a DebuggerClient is connected.
 * @param {DebuggerClient} client
 * @return {Promise} Resolves when connected.
 */
function waitUntilClientConnected(client) {
  return new Promise(resolve => {
    client.addOneTimeListener("connected", resolve);
  });
}

/**
 * Connect a debugger client.
 *
 * @param {DebuggerClient}
 * @return {Promise} Resolves to the targetActor form for the selected tab when the client
 *         is connected.
 */
function connectDebuggerClient(client) {
  return client.connect()
    .then(() => client.listTabs())
    .then(tabs => {
      return tabs.tabs[tabs.selected];
    });
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
    for (const [add, remove] of [
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

registerCleanupFunction(function tearDown() {
  Services.cookies.removeAll();

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function idleWait(time) {
  return DevToolsUtils.waitForTime(time);
}

function busyWait(time) {
  const start = Date.now();
  // eslint-disable-next-line
  let stack;
  while (Date.now() - start < time) {
    stack = Components.stack;
  }
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
    setTimeout(function() {
      waitUntil(predicate).then(() => resolve(true));
    }, interval);
  });
}

function waitForMarkerType(front, types, predicate,
                           unpackFun = (name, data) => data.markers,
                           eventName = "timeline-data") {
  types = [].concat(types);
  predicate = predicate || function() {
    return true;
  };
  let filteredMarkers = [];
  const { promise, resolve } = defer();

  info("Waiting for markers of type: " + types);

  function handler(name, data) {
    if (typeof name === "string" && name !== "markers") {
      return;
    }

    const markers = unpackFun(name, data);
    info("Got markers: " + JSON.stringify(markers, null, 2));

    filteredMarkers = filteredMarkers.concat(
      markers.filter(m => types.includes(m.name)));

    if (types.every(t => filteredMarkers.some(m => m.name === t)) &&
        predicate(filteredMarkers)) {
      front.off(eventName, handler);
      resolve(filteredMarkers);
    }
  }
  front.on(eventName, handler);

  return promise;
}

function getCookieId(name, domain, path) {
  return `${name}${SEPARATOR_GUID}${domain}${SEPARATOR_GUID}${path}`;
}

/**
 * Trigger DOM activity and wait for the corresponding accessibility event.
 * @param  {Object} emitter   Devtools event emitter, usually a front.
 * @param  {Sting} name       Accessibility event in question.
 * @param  {Function} handler Accessibility event handler function with checks.
 * @param  {Promise} task     A promise that resolves when DOM activity is done.
 */
async function emitA11yEvent(emitter, name, handler, task) {
  const promise = emitter.once(name, handler);
  await task();
  await promise;
}

/**
 * Check that accessibilty front is correct and its attributes are also
 * up-to-date.
 * @param  {Object} front         Accessibility front to be tested.
 * @param  {Object} expected      A map of a11y front properties to be verified.
 * @param  {Object} expectedFront Expected accessibility front.
 */
function checkA11yFront(front, expected, expectedFront) {
  ok(front, "The accessibility front is created");

  if (expectedFront) {
    is(front, expectedFront, "Matching accessibility front");
  }

  for (const key in expected) {
    if (["actions", "states", "attributes"].includes(key)) {
      SimpleTest.isDeeply(front[key], expected[key],
        `Accessible Front has correct ${key}`);
    } else {
      is(front[key], expected[key], `accessibility front has correct ${key}`);
    }
  }
}

function getA11yInitOrShutdownPromise() {
  return new Promise(resolve => {
    const observe = (subject, topic, data) => {
      Services.obs.removeObserver(observe, "a11y-init-or-shutdown");
      resolve(data);
    };
    Services.obs.addObserver(observe, "a11y-init-or-shutdown");
  });
}

/**
 * Wait for accessibility service to shut down. We consider it shut down when
 * an "a11y-init-or-shutdown" event is received with a value of "0".
 */
async function waitForA11yShutdown() {
  if (!Services.appinfo.accessibilityEnabled) {
    return;
  }

  await getA11yInitOrShutdownPromise().then(data =>
    data === "0" ? Promise.resolve() : Promise.reject());
}

/**
 * Wait for accessibility service to initialize. We consider it initialized when
 * an "a11y-init-or-shutdown" event is received with a value of "1".
 */
async function waitForA11yInit() {
  if (Services.appinfo.accessibilityEnabled) {
    return;
  }

  await getA11yInitOrShutdownPromise().then(data =>
    data === "1" ? Promise.resolve() : Promise.reject());
}
