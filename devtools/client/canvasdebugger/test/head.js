/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});

var Services = require("Services");
var promise = require("promise");
var { gDevTools } = require("devtools/client/framework/devtools");
var { DebuggerClient } = require("devtools/shared/client/main");
var { DebuggerServer } = require("devtools/server/main");
var { CallWatcherFront } = require("devtools/shared/fronts/call-watcher");
var { CanvasFront } = require("devtools/shared/fronts/canvas");
var { setTimeout } = require("sdk/timers");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { TargetFactory } = require("devtools/client/framework/target");
var { Toolbox } = require("devtools/client/framework/toolbox");
var { isWebGLSupported } = require("devtools/client/shared/webgl-utils");
var mm = null;

const FRAME_SCRIPT_UTILS_URL = "chrome://devtools/content/shared/frame-script-utils.js";
const EXAMPLE_URL = "http://example.com/browser/devtools/client/canvasdebugger/test/";
const SET_TIMEOUT_URL = EXAMPLE_URL + "doc_settimeout.html";
const NO_CANVAS_URL = EXAMPLE_URL + "doc_no-canvas.html";
const RAF_NO_CANVAS_URL = EXAMPLE_URL + "doc_raf-no-canvas.html";
const SIMPLE_CANVAS_URL = EXAMPLE_URL + "doc_simple-canvas.html";
const SIMPLE_BITMASKS_URL = EXAMPLE_URL + "doc_simple-canvas-bitmasks.html";
const SIMPLE_CANVAS_TRANSPARENT_URL = EXAMPLE_URL + "doc_simple-canvas-transparent.html";
const SIMPLE_CANVAS_DEEP_STACK_URL = EXAMPLE_URL + "doc_simple-canvas-deep-stack.html";
const WEBGL_ENUM_URL = EXAMPLE_URL + "doc_webgl-enum.html";
const WEBGL_BINDINGS_URL = EXAMPLE_URL + "doc_webgl-bindings.html";
const WEBGL_DRAW_ARRAYS = EXAMPLE_URL + "doc_webgl-drawArrays.html";
const WEBGL_DRAW_ELEMENTS = EXAMPLE_URL + "doc_webgl-drawElements.html";
const RAF_BEGIN_URL = EXAMPLE_URL + "doc_raf-begin.html";

// Disable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
var gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", false);

// All tests are asynchronous.
waitForExplicitFinish();

var gToolEnabled = Services.prefs.getBoolPref("devtools.canvasdebugger.enabled");

DevToolsUtils.testing = true;

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  DevToolsUtils.testing = false;
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.canvasdebugger.enabled", gToolEnabled);

  // Some of yhese tests use a lot of memory due to GL contexts, so force a GC
  // to help fragmentation.
  info("Forcing GC after canvas debugger test.");
  Cu.forceGC();
});

/**
 * Call manually in tests that use frame script utils after initializing
 * the shader editor. Call after init but before navigating to different pages.
 */
function loadFrameScripts() {
  mm = gBrowser.selectedBrowser.messageManager;
  mm.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
}

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + aUrl);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;
  let tabContainer = targetBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  targetBrowser.removeTab(aTab);
  return deferred.promise;
}

function handleError(aError) {
  ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  finish();
}

var gRequiresWebGL = false;

function ifTestingSupported() {
  ok(false, "You need to define a 'ifTestingSupported' function.");
  finish();
}

function ifTestingUnsupported() {
  todo(false, "Skipping test because some required functionality isn't supported.");
  finish();
}

function test() {
  let generator = isTestingSupported() ? ifTestingSupported : ifTestingUnsupported;
  Task.spawn(generator).then(null, handleError);
}

function createCanvas() {
  return document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}

function isTestingSupported() {
  if (!gRequiresWebGL) {
    info("This test does not require WebGL support.");
    return true;
  }

  let supported = isWebGLSupported(document);

  info("This test requires WebGL support.");
  info("Apparently, WebGL is" + (supported ? "" : " not") + " supported.");
  return supported;
}

function once(aTarget, aEventName, aUseCapture = false) {
  info("Waiting for event: '" + aEventName + "' on " + aTarget + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["on", "off"], // Use event emitter before DOM events for consistency
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"]
  ]) {
    if ((add in aTarget) && (remove in aTarget)) {
      aTarget[add](aEventName, function onEvent(...aArgs) {
        aTarget[remove](aEventName, onEvent, aUseCapture);
        deferred.resolve(...aArgs);
      }, aUseCapture);
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

function navigateInHistory(aTarget, aDirection, aWaitForTargetEvent = "navigate") {
  executeSoon(() => content.history[aDirection]());
  return once(aTarget, aWaitForTargetEvent);
}

function navigate(aTarget, aUrl, aWaitForTargetEvent = "navigate") {
  executeSoon(() => aTarget.activeTab.navigateTo(aUrl));
  return once(aTarget, aWaitForTargetEvent);
}

function reload(aTarget, aWaitForTargetEvent = "navigate") {
  executeSoon(() => aTarget.activeTab.reload());
  return once(aTarget, aWaitForTargetEvent);
}

function initServer() {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }
}

function initCallWatcherBackend(aUrl) {
  info("Initializing a call watcher front.");
  initServer();

  return Task.spawn(function* () {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    let front = new CallWatcherFront(target.client, target.form);
    return { target, front };
  });
}

function initCanvasDebuggerBackend(aUrl) {
  info("Initializing a canvas debugger front.");
  initServer();

  return Task.spawn(function* () {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    let front = new CanvasFront(target.client, target.form);
    return { target, front };
  });
}

function initCanvasDebuggerFrontend(aUrl) {
  info("Initializing a canvas debugger pane.");

  return Task.spawn(function* () {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    Services.prefs.setBoolPref("devtools.canvasdebugger.enabled", true);
    let toolbox = yield gDevTools.showToolbox(target, "canvasdebugger");
    let panel = toolbox.getCurrentPanel();
    return { target, panel };
  });
}

function teardown({target}) {
  info("Destroying the specified canvas debugger.");

  let {tab} = target;
  return gDevTools.closeToolbox(target).then(() => {
    removeTab(tab);
  });
}

/**
 * Takes a string `script` and evaluates it directly in the content
 * in potentially a different process.
 */
function evalInDebuggee(script) {
  let deferred = promise.defer();

  if (!mm) {
    throw new Error("`loadFrameScripts()` must be called when using MessageManager.");
  }

  let id = generateUUID().toString();
  mm.sendAsyncMessage("devtools:test:eval", { script: script, id: id });
  mm.addMessageListener("devtools:test:eval:response", handler);

  function handler({ data }) {
    if (id !== data.id) {
      return;
    }

    mm.removeMessageListener("devtools:test:eval:response", handler);
    deferred.resolve(data.value);
  }

  return deferred.promise;
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item ? item.value : null;
}

/**
 * Waits until a predicate returns true.
 *
 * @param function predicate
 *        Invoked once in a while until it returns true.
 * @param number interval [optional]
 *        How often the predicate is invoked, in milliseconds.
 */
function* waitUntil(predicate, interval = 10) {
  if (yield predicate()) {
    return Promise.resolve(true);
  }
  let deferred = Promise.defer();
  setTimeout(function () {
    waitUntil(predicate).then(() => deferred.resolve(true));
  }, interval);
  return deferred.promise;
}
