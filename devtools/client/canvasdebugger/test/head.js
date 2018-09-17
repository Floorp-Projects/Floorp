/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

var { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { DebuggerServer } = require("devtools/server/main");
var { CallWatcherFront } = require("devtools/shared/fronts/call-watcher");
var { CanvasFront } = require("devtools/shared/fronts/canvas");
var { Toolbox } = require("devtools/client/framework/toolbox");
var { isWebGLSupported } = require("devtools/client/shared/webgl-utils");

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

var gToolEnabled = Services.prefs.getBoolPref("devtools.canvasdebugger.enabled");

registerCleanupFunction(() => {
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.canvasdebugger.enabled", gToolEnabled);

  // Some of yhese tests use a lot of memory due to GL contexts, so force a GC
  // to help fragmentation.
  info("Forcing GC after canvas debugger test.");
  Cu.forceGC();
});

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

async function test() {
  const generator = isTestingSupported() ? ifTestingSupported : ifTestingUnsupported;
  try {
    await generator();
  } catch (e) {
    handleError(e);
  }
}

function createCanvas() {
  return document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}

function isTestingSupported() {
  if (!gRequiresWebGL) {
    info("This test does not require WebGL support.");
    return true;
  }

  const supported = isWebGLSupported(document);

  info("This test requires WebGL support.");
  info("Apparently, WebGL is" + (supported ? "" : " not") + " supported.");
  return supported;
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
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
}

function initCallWatcherBackend(aUrl) {
  info("Initializing a call watcher front.");
  initServer();

  return (async function() {
    const tab = await addTab(aUrl);
    const target = TargetFactory.forTab(tab);

    await target.makeRemote();

    const front = new CallWatcherFront(target.client, target.form);
    return { target, front };
  })();
}

function initCanvasDebuggerBackend(aUrl) {
  info("Initializing a canvas debugger front.");
  initServer();

  return (async function() {
    const tab = await addTab(aUrl);
    const target = TargetFactory.forTab(tab);

    await target.makeRemote();

    const front = new CanvasFront(target.client, target.form);
    return { target, front };
  })();
}

function initCanvasDebuggerFrontend(aUrl) {
  info("Initializing a canvas debugger pane.");

  return (async function() {
    const tab = await addTab(aUrl);
    const target = TargetFactory.forTab(tab);

    await target.makeRemote();

    Services.prefs.setBoolPref("devtools.canvasdebugger.enabled", true);
    const toolbox = await gDevTools.showToolbox(target, "canvasdebugger");
    const panel = toolbox.getCurrentPanel();
    return { target, panel };
  })();
}

function teardown({target}) {
  info("Destroying the specified canvas debugger.");

  const {tab} = target;
  return gDevTools.closeToolbox(target).then(() => {
    removeTab(tab);
  });
}

function getSourceActor(aSources, aURL) {
  const item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item ? item.value : null;
}
