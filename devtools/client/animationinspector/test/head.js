/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const {gDevTools} = Cu.import("resource://devtools/client/framework/gDevTools.jsm", {});
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const promise = require("promise");
const {TargetFactory} = require("devtools/client/framework/target");
const {console} = Cu.import("resource://gre/modules/Console.jsm", {});
const {ViewHelpers} = Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm", {});
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

// All tests are asynchronous
waitForExplicitFinish();

const TEST_URL_ROOT = "http://example.com/browser/devtools/client/animationinspector/test/";
const ROOT_TEST_DIR = getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL = ROOT_TEST_DIR + "doc_frame_script.js";
const COMMON_FRAME_SCRIPT_URL = "chrome://devtools/content/shared/frame-script-utils.js";
const TAB_NAME = "animationinspector";

// Auto clean-up when a test ends
registerCleanupFunction(function*() {
  yield closeAnimationInspector();

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

// Uncomment this pref to dump all devtools protocol traffic
// Services.prefs.setBoolPref("devtools.debugger.log", true);

// Set the testing flag on DevToolsUtils and reset it when the test ends
DevToolsUtils.testing = true;
registerCleanupFunction(() => DevToolsUtils.testing = false);

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.debugger.log");
});

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTab(url) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = promise.defer();

  window.focus();

  let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
  let browser = tab.linkedBrowser;

  info("Loading the helper frame script " + FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

  info("Loading the helper frame script " + COMMON_FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(COMMON_FRAME_SCRIPT_URL, false);

  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");

    def.resolve(tab);
  }, true);

  return def.promise;
}

/**
 * Reload the current tab location.
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 */
function* reloadTab(inspector) {
  let onNewRoot = inspector.once("new-root");
  yield executeInContent("devtools:test:reload", {}, {}, false);
  yield onNewRoot;
  yield inspector.once("inspector-updated");
}

/**
 * Get the NodeFront for a given css selector, via the protocol
 * @param {String} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves to the NodeFront instance
 */
function getNodeFront(selector, {walker}) {
  return walker.querySelector(walker.rootNode, selector);
}

/*
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector.
 * @param {String|NodeFront}
 *        data The node to select
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not
 *        to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = Task.async(function*(data, inspector, reason="test") {
  info("Selecting the node for '" + data + "'");
  let nodeFront = data;
  if (!data._form) {
    nodeFront = yield getNodeFront(data, inspector);
  }
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;

  // 99% of the times, selectNode is called to select an animated node, and we
  // want to make sure the rest of the test waits for the animations to be
  // properly displayed (wait for all target DOM nodes to be previewed).
  // Even if there are no animations, this is safe to do.
  let {AnimationsPanel} = inspector.sidebar.getWindowForTab(TAB_NAME);
  yield waitForAllAnimationTargets(AnimationsPanel);
});

/**
 * Check if there are the expected number of animations being displayed in the
 * panel right now.
 * @param {AnimationsPanel} panel
 * @param {Number} nbAnimations The expected number of animations.
 * @param {String} msg An optional string to be used as the assertion message.
 */
function assertAnimationsDisplayed(panel, nbAnimations, msg = "") {
  msg = msg || `There are ${nbAnimations} animations in the panel`;
  is(panel.animationsTimelineComponent
          .animationsEl
          .querySelectorAll(".animation").length, nbAnimations, msg);
}

/**
 * Takes an Inspector panel that was just created, and waits
 * for a "inspector-updated" event as well as the animation inspector
 * sidebar to be ready. Returns a promise once these are completed.
 *
 * @param {InspectorPanel} inspector
 * @return {Promise}
 */
var waitForAnimationInspectorReady = Task.async(function*(inspector) {
  let win = inspector.sidebar.getWindowForTab(TAB_NAME);
  let updated = inspector.once("inspector-updated");

  // In e10s, if we wait for underlying toolbox actors to
  // load (by setting DevToolsUtils.testing to true), we miss the
  // "animationinspector-ready" event on the sidebar, so check to see if the
  // iframe is already loaded.
  let tabReady = win.document.readyState === "complete" ?
                 promise.resolve() :
                 inspector.sidebar.once("animationinspector-ready");

  return promise.all([updated, tabReady]);
});

/**
 * Open the toolbox, with the inspector tool visible and the animationinspector
 * sidebar selected.
 * @return a promise that resolves when the inspector is ready.
 */
var openAnimationInspector = Task.async(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  info("Opening the toolbox with the inspector selected");
  let toolbox = yield gDevTools.showToolbox(target, "inspector");

  info("Switching to the animationinspector");
  let inspector = toolbox.getPanel("inspector");

  let panelReady = waitForAnimationInspectorReady(inspector);

  info("Waiting for toolbox focus");
  yield waitForToolboxFrameFocus(toolbox);

  inspector.sidebar.select(TAB_NAME);

  info("Waiting for the inspector and sidebar to be ready");
  yield panelReady;

  let win = inspector.sidebar.getWindowForTab(TAB_NAME);
  let {AnimationsController, AnimationsPanel} = win;

  info("Waiting for the animation controller and panel to be ready");
  if (AnimationsPanel.initialized) {
    yield AnimationsPanel.initialized;
  } else {
    yield AnimationsPanel.once(AnimationsPanel.PANEL_INITIALIZED);
  }

  // Make sure we wait for all animations to be loaded (especially their target
  // nodes to be lazily displayed). This is safe to do even if there are no
  // animations displayed.
  yield waitForAllAnimationTargets(AnimationsPanel);

  return {
    toolbox: toolbox,
    inspector: inspector,
    controller: AnimationsController,
    panel: AnimationsPanel,
    window: win
  };
});

/**
 * Close the toolbox.
 * @return a promise that resolves when the toolbox has closed.
 */
var closeAnimationInspector = Task.async(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);
});

/**
 * Wait for the toolbox frame to receive focus after it loads
 * @param {Toolbox} toolbox
 * @return a promise that resolves when focus has been received
 */
function waitForToolboxFrameFocus(toolbox) {
  info("Making sure that the toolbox's frame is focused");
  let def = promise.defer();
  let win = toolbox.frame.contentWindow;
  waitForFocus(def.resolve, win);
  return def.promise;
}

/**
 * Checks whether the inspector's sidebar corresponding to the given id already
 * exists
 * @param {InspectorPanel}
 * @param {String}
 * @return {Boolean}
 */
function hasSideBarTab(inspector, id) {
  return !!inspector.sidebar.getWindowForTab(id);
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for add/removeEventListener
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
        target[remove](eventName, onEvent, useCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

/**
 * Wait for a content -> chrome message on the message manager (the window
 * messagemanager is used).
 * @param {String} name The message name
 * @return {Promise} A promise that resolves to the response data when the
 * message has been received
 */
function waitForContentMessage(name) {
  info("Expecting message " + name + " from content");

  let mm = gBrowser.selectedBrowser.messageManager;

  let def = promise.defer();
  mm.addMessageListener(name, function onMessage(msg) {
    mm.removeMessageListener(name, onMessage);
    def.resolve(msg.data);
  });
  return def.promise;
}

/**
 * Send an async message to the frame script (chrome -> content) and wait for a
 * response message with the same name (content -> chrome).
 * @param {String} name The message name. Should be one of the messages defined
 * in doc_frame_script.js
 * @param {Object} data Optional data to send along
 * @param {Object} objects Optional CPOW objects to send along
 * @param {Boolean} expectResponse If set to false, don't wait for a response
 * with the same name from the content script. Defaults to true.
 * @return {Promise} Resolves to the response data if a response is expected,
 * immediately resolves otherwise
 */
function executeInContent(name, data={}, objects={}, expectResponse=true) {
  info("Sending message " + name + " to content");
  let mm = gBrowser.selectedBrowser.messageManager;

  mm.sendAsyncMessage(name, data, objects);
  if (expectResponse) {
    return waitForContentMessage(name);
  }

  return promise.resolve();
}

/**
 * Get the current playState of an animation player on a given node.
 */
var getAnimationPlayerState = Task.async(function*(selector, animationIndex=0) {
  let playState = yield executeInContent("Test:GetAnimationPlayerState",
                                         {selector, animationIndex});
  return playState;
});

/**
 * Is the given node visible in the page (rendered in the frame tree).
 * @param {DOMNode}
 * @return {Boolean}
 */
function isNodeVisible(node) {
  return !!node.getClientRects().length;
}

/**
 * Wait for all AnimationTargetNode instances to be fully loaded
 * (fetched their related actor and rendered), and return them.
 * @param {AnimationsPanel} panel
 * @return {Array} all AnimationTargetNode instances
 */
var waitForAllAnimationTargets = Task.async(function*(panel) {
  let targets = panel.animationsTimelineComponent.targetNodes;
  yield promise.all(targets.map(t => {
    if (!t.nodeFront) {
      return t.once("target-retrieved");
    }
    return false;
  }));
  return targets;
});

/**
 * Check the scrubber element in the timeline is moving.
 * @param {AnimationPanel} panel
 * @param {Boolean} isMoving
 */
function* assertScrubberMoving(panel, isMoving) {
  let timeline = panel.animationsTimelineComponent;
  let scrubberEl = timeline.scrubberEl;

  if (isMoving) {
    // If we expect the scrubber to move, just wait for a couple of
    // timeline-data-changed events and compare times.
    let {time: time1} = yield timeline.once("timeline-data-changed");
    let {time: time2} = yield timeline.once("timeline-data-changed");
    ok(time2 > time1, "The scrubber is moving");
  } else {
    // If instead we expect the scrubber to remain at its position, just wait
    // for some time and make sure timeline-data-changed isn't emitted.
    let hasMoved = false;
    timeline.once("timeline-data-changed", () => hasMoved = true);
    yield new Promise(r => setTimeout(r, 500));
    ok(!hasMoved, "The scrubber is not moving");
  }
}

/**
 * Click the play/pause button in the timeline toolbar and wait for animations
 * to update.
 * @param {AnimationsPanel} panel
 */
function* clickTimelinePlayPauseButton(panel) {
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);

  let btn = panel.playTimelineButtonEl;
  let win = btn.ownerDocument.defaultView;
  EventUtils.sendMouseEvent({type: "click"}, btn, win);

  yield onUiUpdated;
  yield waitForAllAnimationTargets(panel);
}

/**
 * Click the rewind button in the timeline toolbar and wait for animations to
 * update.
 * @param {AnimationsPanel} panel
 */
function* clickTimelineRewindButton(panel) {
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);

  let btn = panel.rewindTimelineButtonEl;
  let win = btn.ownerDocument.defaultView;
  EventUtils.sendMouseEvent({type: "click"}, btn, win);

  yield onUiUpdated;
  yield waitForAllAnimationTargets(panel);
}

/**
 * Select a rate inside the playback rate selector in the timeline toolbar and
 * wait for animations to update.
 * @param {AnimationsPanel} panel
 * @param {Number} rate The new rate value to be selected
 */
function* changeTimelinePlaybackRate(panel, rate) {
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);

  let select = panel.rateSelectorEl.firstChild;
  let win = select.ownerDocument.defaultView;

  // Get the right option.
  let option = [...select.options].filter(o => o.value === rate + "")[0];
  if (!option) {
    ok(false,
       "Could not find an option for rate " + rate + " in the rate selector. " +
       "Values are: " + [...select.options].map(o => o.value));
    return;
  }

  // Simulate the right events to select the option in the drop-down.
  EventUtils.synthesizeMouseAtCenter(select, {type: "mousedown"}, win);
  EventUtils.synthesizeMouseAtCenter(option, {type: "mouseup"}, win);

  yield onUiUpdated;
  yield waitForAllAnimationTargets(panel);

  // Simulate a mousemove outside of the rate selector area to avoid subsequent
  // tests from failing because of unwanted mouseover events.
  EventUtils.synthesizeMouseAtCenter(win.document.querySelector("#timeline-toolbar"),
                                     {type: "mousemove"}, win);
}

/**
 * Prevent the toolbox common highlighter from making backend requests.
 * @param {Toolbox} toolbox
 */
function disableHighlighter(toolbox) {
  toolbox._highlighter = {
    showBoxModel: () => new Promise(r => r()),
    hideBoxModel: () => new Promise(r => r()),
    pick: () => new Promise(r => r()),
    cancelPick: () => new Promise(r => r()),
    destroy: () => {},
    traits: {}
  };
}

/**
 * Click on an animation in the timeline to select/unselect it.
 * @param {AnimationsPanel} panel The panel instance.
 * @param {Number} index The index of the animation to click on.
 * @param {Boolean} shouldClose Set to true if clicking should close the
 * animation.
 * @return {Promise} resolves to the animation whose state has changed.
 */
function* clickOnAnimation(panel, index, shouldClose) {
  let timeline = panel.animationsTimelineComponent;

  // Expect a selection event.
  let onSelectionChanged = timeline.once(shouldClose
                                         ? "animation-unselected"
                                         : "animation-selected");

  // If we're opening the animation, also wait for the keyframes-retrieved
  // event.
  let onReady = shouldClose
                ? Promise.resolve()
                : timeline.details[index].once("keyframes-retrieved");

  info("Click on animation " + index + " in the timeline");
  let timeBlock = timeline.rootWrapperEl.querySelectorAll(".time-block")[index];
  EventUtils.sendMouseEvent({type: "click"}, timeBlock,
                            timeBlock.ownerDocument.defaultView);

  yield onReady;
  return yield onSelectionChanged;
}

/**
 * Get an instance of the Keyframes component from the timeline.
 * @param {AnimationsPanel} panel The panel instance.
 * @param {Number} animationIndex The index of the animation in the timeline.
 * @param {String} propertyName The name of the animated property.
 * @return {Keyframes} The Keyframes component instance.
 */
function getKeyframeComponent(panel, animationIndex, propertyName) {
  let timeline = panel.animationsTimelineComponent;
  let detailsComponent = timeline.details[animationIndex];
  return detailsComponent.keyframeComponents
                         .find(c => c.propertyName === propertyName);
}

/**
 * Get a keyframe element from the timeline.
 * @param {AnimationsPanel} panel The panel instance.
 * @param {Number} animationIndex The index of the animation in the timeline.
 * @param {String} propertyName The name of the animated property.
 * @param {Index} keyframeIndex The index of the keyframe.
 * @return {DOMNode} The keyframe element.
 */
function getKeyframeEl(panel, animationIndex, propertyName, keyframeIndex) {
  let keyframeComponent = getKeyframeComponent(panel, animationIndex, propertyName);
  return keyframeComponent.keyframesEl.querySelectorAll(".frame")[keyframeIndex];
}
