/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

var gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
// To enable logging for try runs, just set the pref to true.
Services.prefs.setBoolPref("devtools.debugger.log", false);

var { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
var { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
var { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

var promise = require("promise");
var { DebuggerClient } = require("devtools/toolkit/client/main");
var { DebuggerServer } = require("devtools/server/main");
var { WebGLFront } = require("devtools/server/actors/webgl");
var DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
var TiltGL = require("devtools/tilt/tilt-gl");
var {TargetFactory} = require("devtools/framework/target");
var {Toolbox} = require("devtools/framework/toolbox");
var mm = null;

const FRAME_SCRIPT_UTILS_URL = "chrome://devtools/content/shared/frame-script-utils.js"
const EXAMPLE_URL = "http://example.com/browser/browser/devtools/shadereditor/test/";
const SIMPLE_CANVAS_URL = EXAMPLE_URL + "doc_simple-canvas.html";
const SHADER_ORDER_URL = EXAMPLE_URL + "doc_shader-order.html";
const MULTIPLE_CONTEXTS_URL = EXAMPLE_URL + "doc_multiple-contexts.html";
const OVERLAPPING_GEOMETRY_CANVAS_URL = EXAMPLE_URL + "doc_overlapping-geometry.html";
const BLENDED_GEOMETRY_CANVAS_URL = EXAMPLE_URL + "doc_blended-geometry.html";

// All tests are asynchronous.
waitForExplicitFinish();

var gToolEnabled = Services.prefs.getBoolPref("devtools.shadereditor.enabled");

DevToolsUtils.testing = true;

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  DevToolsUtils.testing = false;
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.shadereditor.enabled", gToolEnabled);

  // These tests use a lot of memory due to GL contexts, so force a GC to help
  // fragmentation.
  info("Forcing GC after shadereditor test.");
  Cu.forceGC();
});

/**
 * Call manually in tests that use frame script utils after initializing
 * the shader editor. Must be called after initializing so we can detect
 * whether or not `content` is a CPOW or not. Call after init but before navigating
 * to different pages, as bfcache and thus shader caching gets really strange if
 * frame script attached in the middle of the test.
 */
function loadFrameScripts () {
  if (Cu.isCrossProcessWrapper(content)) {
    mm = gBrowser.selectedBrowser.messageManager;
    mm.loadFrameScript(FRAME_SCRIPT_UTILS_URL, false);
  }
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

function ifWebGLSupported() {
  ok(false, "You need to define a 'ifWebGLSupported' function.");
  finish();
}

function ifWebGLUnsupported() {
  todo(false, "Skipping test because WebGL isn't supported.");
  finish();
}

function test() {
  let generator = isWebGLSupported() ? ifWebGLSupported : ifWebGLUnsupported;
  Task.spawn(generator).then(null, handleError);
}

function createCanvas() {
  return document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}

function isWebGLSupported() {
  let supported =
    !TiltGL.isWebGLForceEnabled() &&
     TiltGL.isWebGLSupported() &&
     TiltGL.create3DContext(createCanvas());

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

// Hack around `once`, as that only resolves to a single (first) argument
// and discards the rest. `onceSpread` is similar, except resolves to an
// array of all of the arguments in the handler. These should be consolidated
// into the same function, but many tests will need to be changed.
function onceSpread(aTarget, aEvent) {
  let deferred = promise.defer();
  aTarget.once(aEvent, (...args) => deferred.resolve(args));
  return deferred.promise;
}

function observe(aNotificationName, aOwnsWeak = false) {
  info("Waiting for observer notification: '" + aNotificationName + ".");

  let deferred = promise.defer();

  Services.obs.addObserver(function onNotification(...aArgs) {
    Services.obs.removeObserver(onNotification, aNotificationName);
    deferred.resolve.apply(deferred, aArgs);
  }, aNotificationName, aOwnsWeak);

  return deferred.promise;
}

function isApprox(aFirst, aSecond, aMargin = 1) {
  return Math.abs(aFirst - aSecond) <= aMargin;
}

function isApproxColor(aFirst, aSecond, aMargin) {
  return isApprox(aFirst.r, aSecond.r, aMargin) &&
    isApprox(aFirst.g, aSecond.g, aMargin) &&
    isApprox(aFirst.b, aSecond.b, aMargin) &&
    isApprox(aFirst.a, aSecond.a, aMargin);
}

function ensurePixelIs (aFront, aPosition, aColor, aWaitFlag = false, aSelector = "canvas") {
  return Task.spawn(function*() {
    let pixel = yield aFront.getPixel({ selector: aSelector, position: aPosition });
    if (isApproxColor(pixel, aColor)) {
      ok(true, "Expected pixel is shown at: " + aPosition.toSource());
      return;
    }

    if (aWaitFlag) {
      yield aFront.waitForFrame();
      return ensurePixelIs(aFront, aPosition, aColor, aWaitFlag, aSelector);
    }

    ok(false, "Expected pixel was not already shown at: " + aPosition.toSource());
    throw new Error("Expected pixel was not already shown at: " + aPosition.toSource());
  });
}

function navigateInHistory(aTarget, aDirection, aWaitForTargetEvent = "navigate") {
  if (Cu.isCrossProcessWrapper(content)) {
    if (!mm) {
      throw new Error("`loadFrameScripts()` must be called before attempting to navigate in e10s.");
    }
    mm.sendAsyncMessage("devtools:test:history", { direction: aDirection });
  }
  else {
    executeSoon(() => content.history[aDirection]());
  }
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

function initBackend(aUrl) {
  info("Initializing a shader editor front.");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    let front = new WebGLFront(target.client, target.form);
    return { target, front };
  });
}

function initShaderEditor(aUrl) {
  info("Initializing a shader editor pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);

    yield target.makeRemote();

    Services.prefs.setBoolPref("devtools.shadereditor.enabled", true);
    let toolbox = yield gDevTools.showToolbox(target, "shadereditor");
    let panel = toolbox.getCurrentPanel();
    return { target, panel };
  });
}

function teardown(aPanel) {
  info("Destroying the specified shader editor.");

  return promise.all([
    once(aPanel, "destroyed"),
    removeTab(aPanel.target.tab)
  ]);
}

// Due to `program-linked` events firing synchronously, we cannot
// just yield/chain them together, as then we miss all actors after the
// first event since they're fired consecutively. This allows us to capture
// all actors and returns an array containing them.
//
// Takes a `front` object that is an event emitter, the number of
// programs that should be listened to and waited on, and an optional
// `onAdd` function that calls with the entire actors array on program link
function getPrograms(front, count, onAdd) {
  let actors = [];
  let deferred = promise.defer();
  front.on("program-linked", function onLink (actor) {
    if (actors.length !== count) {
      actors.push(actor);
      if (typeof onAdd === 'function') onAdd(actors)
    }
    if (actors.length === count) {
      front.off("program-linked", onLink);
      deferred.resolve(actors);
    }
  });
  return deferred.promise;
}
