/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../inspector/test/head.js */
// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

const FRAME_SCRIPT_URL = CHROME_URL_ROOT + "doc_frame_script.js";
const COMMON_FRAME_SCRIPT_URL = "chrome://devtools/content/shared/frame-script-utils.js";
const TAB_NAME = "animationinspector";
const ANIMATION_L10N =
  new LocalizationHelper("devtools/locale/animationinspector.properties");

// Auto clean-up when a test ends
registerCleanupFunction(function* () {
  yield closeAnimationInspector();

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

// WebAnimations API is not enabled by default in all release channels yet, see
// Bug 1264101.
function enableWebAnimationsAPI() {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.animations-api.core.enabled", true]
    ]}, resolve);
  });
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
var _addTab = addTab;
addTab = function (url) {
  return enableWebAnimationsAPI().then(() => _addTab(url)).then(tab => {
    let browser = tab.linkedBrowser;
    info("Loading the helper frame script " + FRAME_SCRIPT_URL);
    browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
    info("Loading the helper frame script " + COMMON_FRAME_SCRIPT_URL);
    browser.messageManager.loadFrameScript(COMMON_FRAME_SCRIPT_URL, false);
    return tab;
  });
};

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

/*
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector and wait for the animations to be displayed
 * @param {String|NodeFront}
 *        data The node to select
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not
 *        to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
           and animations of its subtree are properly displayed.
 */
var selectNodeAndWaitForAnimations = Task.async(
  function* (data, inspector, reason = "test") {
    yield selectNode(data, inspector, reason);

    // We want to make sure the rest of the test waits for the animations to
    // be properly displayed (wait for all target DOM nodes to be previewed).
    let {AnimationsPanel} = inspector.sidebar.getWindowForTab(TAB_NAME);
    yield waitForAllAnimationTargets(AnimationsPanel);
  }
);

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
var waitForAnimationInspectorReady = Task.async(function* (inspector) {
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
var openAnimationInspector = Task.async(function* () {
  let {inspector, toolbox} = yield openInspectorSidebarTab(TAB_NAME);

  info("Waiting for the inspector and sidebar to be ready");
  yield waitForAnimationInspectorReady(inspector);

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
var closeAnimationInspector = Task.async(function* () {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);
});

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

  return new Promise(resolve => {
    mm.addMessageListener(name, function onMessage(msg) {
      mm.removeMessageListener(name, onMessage);
      resolve(msg.data);
    });
  });
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
function executeInContent(name, data = {}, objects = {},
                          expectResponse = true) {
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
var getAnimationPlayerState = Task.async(function* (selector,
                                                    animationIndex = 0) {
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
var waitForAllAnimationTargets = Task.async(function* (panel) {
  let targets = panel.animationsTimelineComponent.targetNodes;
  yield promise.all(targets.map(t => {
    if (!t.previewer.nodeFront) {
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
    timeline.once("timeline-data-changed", () => {
      hasMoved = true;
    });
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
  EventUtils.synthesizeMouseAtCenter(
    win.document.querySelector("#timeline-toolbar"), {type: "mousemove"}, win);
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
  let keyframeComponent = getKeyframeComponent(panel, animationIndex,
                                               propertyName);
  return keyframeComponent.keyframesEl
                          .querySelectorAll(".frame")[keyframeIndex];
}
