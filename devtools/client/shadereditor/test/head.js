/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { DebuggerServer } = require("devtools/server/main");
var { WebGLFront } = require("devtools/shared/fronts/webgl");
var { Toolbox } = require("devtools/client/framework/toolbox");
var { isWebGLSupported } = require("devtools/client/shared/webgl-utils");

const EXAMPLE_URL = "http://example.com/browser/devtools/client/shadereditor/test/";
const SIMPLE_CANVAS_URL = EXAMPLE_URL + "doc_simple-canvas.html";
const SHADER_ORDER_URL = EXAMPLE_URL + "doc_shader-order.html";
const MULTIPLE_CONTEXTS_URL = EXAMPLE_URL + "doc_multiple-contexts.html";
const OVERLAPPING_GEOMETRY_CANVAS_URL = EXAMPLE_URL + "doc_overlapping-geometry.html";
const BLENDED_GEOMETRY_CANVAS_URL = EXAMPLE_URL + "doc_blended-geometry.html";

var gEnableLogging = Services.prefs.getBoolPref("devtools.debugger.log");
// To enable logging for try runs, just set the pref to true.
Services.prefs.setBoolPref("devtools.debugger.log", false);

var gToolEnabled = Services.prefs.getBoolPref("devtools.shadereditor.enabled");

registerCleanupFunction(() => {
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
function loadFrameScripts() {
  if (!content) {
    loadFrameScriptUtils();
  }
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

async function test() {
  const generator = isWebGLSupported(document) ? ifWebGLSupported : ifWebGLUnsupported;
  try {
    await generator();
  } catch (e) {
    handleError(e);
  }
}

function createCanvas() {
  return document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}

function observe(aNotificationName, aOwnsWeak = false) {
  info("Waiting for observer notification: '" + aNotificationName + ".");

  const deferred = defer();

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

function ensurePixelIs(aFront, aPosition, aColor, aWaitFlag = false, aSelector = "canvas") {
  return (async function() {
    const pixel = await aFront.getPixel({ selector: aSelector, position: aPosition });
    if (isApproxColor(pixel, aColor)) {
      ok(true, "Expected pixel is shown at: " + aPosition.toSource());
      return;
    }

    if (aWaitFlag) {
      await aFront.waitForFrame();
      await ensurePixelIs(aFront, aPosition, aColor, aWaitFlag, aSelector);
      return;
    }

    ok(false, "Expected pixel was not already shown at: " + aPosition.toSource());
    throw new Error("Expected pixel was not already shown at: " + aPosition.toSource());
  })();
}

function navigateInHistory(aTarget, aDirection, aWaitForTargetEvent = "navigate") {
  if (!content) {
    const mm = gBrowser.selectedBrowser.messageManager;
    mm.sendAsyncMessage("devtools:test:history", { direction: aDirection });
  } else {
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

  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  return (async function() {
    const tab = await addTab(aUrl);
    const target = TargetFactory.forTab(tab);

    await target.makeRemote();

    const front = new WebGLFront(target.client, target.form);
    return { target, front };
  })();
}

function initShaderEditor(aUrl) {
  info("Initializing a shader editor pane.");

  return (async function() {
    const tab = await addTab(aUrl);
    const target = TargetFactory.forTab(tab);

    await target.makeRemote();

    Services.prefs.setBoolPref("devtools.shadereditor.enabled", true);
    const toolbox = await gDevTools.showToolbox(target, "shadereditor");
    const panel = toolbox.getCurrentPanel();
    return { target, panel };
  })();
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
  const actors = [];
  const deferred = defer();
  front.on("program-linked", function onLink(actor) {
    if (actors.length !== count) {
      actors.push(actor);
      if (typeof onAdd === "function") {
        onAdd(actors);
      }
    }
    if (actors.length === count) {
      front.off("program-linked", onLink);
      deferred.resolve(actors);
    }
  });
  return deferred.promise;
}
