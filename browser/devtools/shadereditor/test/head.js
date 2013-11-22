/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Enable logging for all the tests. Both the debugger server and frontend will
// be affected by this pref.
let gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
Services.prefs.setBoolPref("devtools.debugger.log", true);

let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});
let { Promise: promise } = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});
let { DebuggerClient } = Cu.import("resource://gre/modules/devtools/dbg-client.jsm", {});

let { WebGLFront } = devtools.require("devtools/server/actors/webgl");
let TiltGL = devtools.require("devtools/tilt/tilt-gl");
let TargetFactory = devtools.TargetFactory;
let Toolbox = devtools.Toolbox;

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/shadereditor/test/";
const SIMPLE_CANVAS_URL = EXAMPLE_URL + "doc_simple-canvas.html";
const SHADER_ORDER_URL = EXAMPLE_URL + "doc_shader-order.html";
const MULTIPLE_CONTEXTS_URL = EXAMPLE_URL + "doc_multiple-contexts.html";
const OVERLAPPING_GEOMETRY_CANVAS_URL = EXAMPLE_URL + "doc_overlapping-geometry.html";

// All tests are asynchronous.
waitForExplicitFinish();

let gToolEnabled = Services.prefs.getBoolPref("devtools.shadereditor.enabled");

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.shadereditor.enabled", gToolEnabled);

  // These tests use a lot of memory due to GL contexts, so force a GC to help
  // fragmentation.
  info("Forcing GC after shadereditor test.");
  Cu.forceGC();
});

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
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in aTarget) && (remove in aTarget)) {
      aTarget[add](aEventName, function onEvent(...aArgs) {
        aTarget[remove](aEventName, onEvent, aUseCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, aUseCapture);
      break;
    }
  }

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

function waitForFrame(aDebuggee) {
  let deferred = promise.defer();
  aDebuggee.requestAnimationFrame(deferred.resolve);
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

function getPixels(aDebuggee, aSelector = "canvas") {
  let canvas = aDebuggee.document.querySelector(aSelector);
  let gl = canvas.getContext("webgl");

  let { width, height } = canvas;
  let buffer = new aDebuggee.Uint8Array(width * height * 4);
  gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buffer);

  info("Retrieved pixels: " + width + "x" + height);
  return [buffer, width, height];
}

function getPixel(aDebuggee, aPosition, aSelector = "canvas") {
  let canvas = aDebuggee.document.querySelector(aSelector);
  let gl = canvas.getContext("webgl");

  let { width, height } = canvas;
  let { x, y } = aPosition;
  let buffer = new aDebuggee.Uint8Array(4);
  gl.readPixels(x, height - y - 1, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, buffer);

  let pixel = { r: buffer[0], g: buffer[1], b: buffer[2], a: buffer[3] };

  info("Retrieved pixel: " + pixel.toSource() + " at " + aPosition.toSource());
  return pixel;
}

function ensurePixelIs(aDebuggee, aPosition, aColor, aWaitFlag = false, aSelector = "canvas") {
  let pixel = getPixel(aDebuggee, aPosition, aSelector);
  if (isApproxColor(pixel, aColor)) {
    ok(true, "Expected pixel is shown at: " + aPosition.toSource());
    return promise.resolve(null);
  }
  if (aWaitFlag) {
    return Task.spawn(function() {
      yield waitForFrame(aDebuggee);
      yield ensurePixelIs(aDebuggee, aPosition, aColor, aWaitFlag, aSelector);
    });
  }
  ok(false, "Expected pixel was not already shown at: " + aPosition.toSource());
  return promise.reject(null);
}

function navigateInHistory(aTarget, aDirection, aWaitForTargetEvent = "navigate") {
  executeSoon(() => content.history[aDirection]());
  return once(aTarget, aWaitForTargetEvent);
}

function navigate(aTarget, aUrl, aWaitForTargetEvent = "navigate") {
  executeSoon(() => aTarget.client.activeTab.navigateTo(aUrl));
  return once(aTarget, aWaitForTargetEvent);
}

function reload(aTarget, aWaitForTargetEvent = "navigate") {
  executeSoon(() => aTarget.client.activeTab.reload());
  return once(aTarget, aWaitForTargetEvent);
}

function initBackend(aUrl) {
  info("Initializing a shader editor front.");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(() => true);
    DebuggerServer.addBrowserActors();
  }

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);
    let debuggee = target.window.wrappedJSObject;

    yield target.makeRemote();

    let front = new WebGLFront(target.client, target.form);
    return [target, debuggee, front];
  });
}

function initShaderEditor(aUrl) {
  info("Initializing a shader editor pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);
    let debuggee = target.window.wrappedJSObject;

    yield target.makeRemote();

    Services.prefs.setBoolPref("devtools.shadereditor.enabled", true);
    let toolbox = yield gDevTools.showToolbox(target, "shadereditor");
    let panel = toolbox.getCurrentPanel();
    return [target, debuggee, panel];
  });
}

function teardown(aPanel) {
  info("Destroying the specified shader editor.");

  return promise.all([
    once(aPanel, "destroyed"),
    removeTab(aPanel.target.tab)
  ]);
}
