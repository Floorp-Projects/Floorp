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
let { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
let { gDevTools } = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

let { WebAudioFront } = devtools.require("devtools/server/actors/webaudio");
let TargetFactory = devtools.TargetFactory;

const EXAMPLE_URL = "http://example.com/browser/browser/devtools/webaudioeditor/test/";
const SIMPLE_CONTEXT_URL = EXAMPLE_URL + "doc_simple-context.html";
const COMPLEX_CONTEXT_URL = EXAMPLE_URL + "doc_complex-context.html";
const SIMPLE_NODES_URL = EXAMPLE_URL + "doc_simple-node-creation.html";

// All tests are asynchronous.
waitForExplicitFinish();

let gToolEnabled = Services.prefs.getBoolPref("devtools.webaudioeditor.enabled");

registerCleanupFunction(() => {
  info("finish() was called, cleaning up...");
  Services.prefs.setBoolPref("devtools.debugger.log", gEnableLogging);
  Services.prefs.setBoolPref("devtools.webaudioeditor.enabled", gToolEnabled);
  Cu.forceGC();
});

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = Promise.defer();
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

  let deferred = Promise.defer();
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

function once(aTarget, aEventName, aUseCapture = false) {
  info("Waiting for event: '" + aEventName + "' on " + aTarget + ".");

  let deferred = Promise.defer();

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

function reload(aTarget, aWaitForTargetEvent = "navigate") {
  aTarget.activeTab.reload();
  return once(aTarget, aWaitForTargetEvent);
}

function test () {
  Task.spawn(spawnTest).then(finish, handleError);
}

function initBackend(aUrl) {
  info("Initializing a web audio editor front.");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init(() => true);
    DebuggerServer.addBrowserActors();
  }

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);
    let debuggee = target.window.wrappedJSObject;

    yield target.makeRemote();

    let front = new WebAudioFront(target.client, target.form);
    return [target, debuggee, front];
  });
}

function initWebAudioEditor(aUrl) {
  info("Initializing a web audio editor pane.");

  return Task.spawn(function*() {
    let tab = yield addTab(aUrl);
    let target = TargetFactory.forTab(tab);
    let debuggee = target.window.wrappedJSObject;

    yield target.makeRemote();

    Services.prefs.setBoolPref("devtools.webaudioeditor.enabled", true);
    let toolbox = yield gDevTools.showToolbox(target, "webaudioeditor");
    let panel = toolbox.getCurrentPanel();
    return [target, debuggee, panel];
  });
}

function teardown(aPanel) {
  info("Destroying the web audio editor.");

  return Promise.all([
    once(aPanel, "destroyed"),
    removeTab(aPanel.target.tab)
  ]).then(() => {
    let gBrowser = window.gBrowser;
    while (gBrowser.tabs.length > 1) {
      gBrowser.removeCurrentTab();
    }
    gBrowser = null;
  });
}

// Due to web audio will fire most events synchronously back-to-back,
// and we can't yield them in a chain without missing actors, this allows
// us to listen for `n` events and return a promise resolving to them.
//
// Takes a `front` object that is an event emitter, the number of
// programs that should be listened to and waited on, and an optional
// `onAdd` function that calls with the entire actors array on program link
function getN (front, eventName, count, spread) {
  let actors = [];
  let deferred = Promise.defer();
  front.on(eventName, function onEvent (...args) {
    let actor = args[0];
    if (actors.length !== count) {
      actors.push(spread ? args : actor);
    }
    if (actors.length === count) {
      front.off(eventName, onEvent);
      deferred.resolve(actors);
    }
  });
  return deferred.promise;
}

function get (front, eventName) { return getN(front, eventName, 1); }
function get2 (front, eventName) { return getN(front, eventName, 2); }
function get3 (front, eventName) { return getN(front, eventName, 3); }
function getSpread (front, eventName) { return getN(front, eventName, 1, true); }
function get2Spread (front, eventName) { return getN(front, eventName, 2, true); }
function get3Spread (front, eventName) { return getN(front, eventName, 3, true); }
function getNSpread (front, eventName, count) { return getN(front, eventName, count, true); }

/**
 * Waits for the UI_GRAPH_RENDERED event to fire, but only
 * resolves when the graph was rendered with the correct count of
 * nodes and edges.
 */
function waitForGraphRendered (front, nodeCount, edgeCount) {
  let deferred = Promise.defer();
  let eventName = front.EVENTS.UI_GRAPH_RENDERED;
  front.on(eventName, function onGraphRendered (_, nodes, edges) {
    if (nodes === nodeCount && edges === edgeCount) {
      front.off(eventName, onGraphRendered);
      deferred.resolve();
    }
  });
  return deferred.promise;
}

function checkVariableView (view, index, hash) {
  let scope = view.getScopeAtIndex(index);
  let variables = Object.keys(hash);
  variables.forEach(variable => {
    let aVar = scope.get(variable);
    is(aVar.target.querySelector(".name").getAttribute("value"), variable,
      "Correct property name for " + variable);
    is(aVar.target.querySelector(".value").getAttribute("value"), hash[variable],
      "Correct property value of " + hash[variable] + " for " + variable);
  });
}

function modifyVariableView (win, view, index, prop, value) {
  let deferred = Promise.defer();
  let scope = view.getScopeAtIndex(index);
  let aVar = scope.get(prop);
  scope.expand();

  // Must wait for the scope DOM to be available to receive
  // events
  executeSoon(() => {
    let varValue = aVar.target.querySelector(".title > .value");
    EventUtils.sendMouseEvent({ type: "mousedown" }, varValue, win);

    win.on(win.EVENTS.UI_SET_PARAM, handleSetting);
    win.on(win.EVENTS.UI_SET_PARAM_ERROR, handleSetting);

    info("Setting " + value + " for " + prop + "....");
    let varInput = aVar.target.querySelector(".title > .element-value-input");
    setText(varInput, value);
    EventUtils.sendKey("RETURN", win);
  });

  function handleSetting (eventName) {
    win.off(win.EVENTS.UI_SET_PARAM, handleSetting);
    win.off(win.EVENTS.UI_SET_PARAM_ERROR, handleSetting);
    if (eventName === win.EVENTS.UI_SET_PARAM)
      deferred.resolve();
    if (eventName === win.EVENTS.UI_SET_PARAM_ERROR)
      deferred.reject();
  }

  return deferred.promise;
}

function clearText (aElement) {
  info("Clearing text...");
  aElement.focus();
  aElement.value = "";
}

function setText (aElement, aText) {
  clearText(aElement);
  info("Setting text: " + aText);
  aElement.value = aText;
}

function findGraphEdge (win, source, target) {
  let selector = ".edgePaths .edgePath[data-source='" + source + "'][data-target='" + target + "']";
  return win.document.querySelector(selector);
}

function findGraphNode (win, node) {
  let selector = ".nodes > g[data-id='" + node + "']";
  return win.document.querySelector(selector);
}

function click (win, element) {
  EventUtils.sendMouseEvent({ type: "click" }, element, win);
}

function mouseOver (win, element) {
  EventUtils.sendMouseEvent({ type: "mouseover" }, element, win);
}

/**
 * List of audio node properties to test against expectations of the AudioNode actor
 */

const NODE_PROPERTIES = {
  "OscillatorNode": ["type", "frequency", "detune"],
  "GainNode": ["gain"],
  "DelayNode": ["delayTime"],
  "AudioBufferSourceNode": ["buffer", "playbackRate", "loop", "loopStart", "loopEnd"],
  "ScriptProcessorNode": ["bufferSize"],
  "PannerNode": ["panningModel", "distanceModel", "refDistance", "maxDistance", "rolloffFactor", "coneInnerAngle", "coneOuterAngle", "coneOuterGain"],
  "ConvolverNode": ["buffer", "normalize"],
  "DynamicsCompressorNode": ["threshold", "knee", "ratio", "reduction", "attack", "release"],
  "BiquadFilterNode": ["type", "frequency", "Q", "detune", "gain"],
  "WaveShaperNode": ["curve", "oversample"],
  "AnalyserNode": ["fftSize", "minDecibels", "maxDecibels", "smoothingTimeConstraint", "frequencyBinCount"],
  "AudioDestinationNode": [],
  "ChannelSplitterNode": [],
  "ChannelMergerNode": []
};
