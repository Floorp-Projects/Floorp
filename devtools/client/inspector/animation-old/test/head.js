/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../test/head.js */
// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this);

const FRAME_SCRIPT_URL = CHROME_URL_ROOT + "doc_frame_script.js";
const TAB_NAME = "animationinspector";
const ANIMATION_L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// Auto clean-up when a test ends
registerCleanupFunction(async function() {
  await closeAnimationInspector();

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

// Disable new animation inspector.
Services.prefs.setBoolPref("devtools.new-animationinspector.enabled", false);

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.new-animationinspector.enabled");
  Services.prefs.clearUserPref("devtools.debugger.log");
});

// Some animation features are not enabled by default in release/beta channels
// yet including:
// * parts of the Web Animations API (Bug 1264101), and
// * the frames() timing function (Bug 1379582).
function enableAnimationFeatures() {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.animations-api.core.enabled", true],
      ["layout.css.frames-timing.enabled", true],
    ]}, resolve);
  });
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
var _addTab = addTab;
addTab = function(url) {
  return enableAnimationFeatures().then(() => _addTab(url)).then(tab => {
    let browser = tab.linkedBrowser;
    info("Loading the helper frame script " + FRAME_SCRIPT_URL);
    browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
    loadFrameScriptUtils(browser);
    return tab;
  });
};

/**
 * Reload the current tab location.
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 */
async function reloadTab(inspector) {
  let onNewRoot = inspector.once("new-root");
  await executeInContent("devtools:test:reload", {}, {}, false);
  await onNewRoot;
  await inspector.once("inspector-updated");
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
var selectNodeAndWaitForAnimations = async function(data, inspector, reason = "test") {
  // We want to make sure the rest of the test waits for the animations to
  // be properly displayed (wait for all target DOM nodes to be previewed).
  let {AnimationsController, AnimationsPanel} =
    inspector.sidebar.getWindowForTab(TAB_NAME);
  let onUiUpdated = AnimationsPanel.once(AnimationsPanel.UI_UPDATED_EVENT);

  await selectNode(data, inspector, reason);

  await onUiUpdated;
  if (AnimationsController.animationPlayers.length !== 0) {
    await waitForAnimationTimelineRendering(AnimationsPanel);
    await waitForAllAnimationTargets(AnimationsPanel);
  }
};

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
var waitForAnimationInspectorReady = async function(inspector) {
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
};

/**
 * Open the toolbox, with the inspector tool visible and the animationinspector
 * sidebar selected.
 * @return a promise that resolves when the inspector is ready.
 */
var openAnimationInspector = async function() {
  let {inspector, toolbox} = await openInspectorSidebarTab(TAB_NAME);

  info("Waiting for the inspector and sidebar to be ready");
  await waitForAnimationInspectorReady(inspector);

  let win = inspector.sidebar.getWindowForTab(TAB_NAME);
  let {AnimationsController, AnimationsPanel} = win;

  info("Waiting for the animation controller and panel to be ready");
  if (AnimationsPanel.initialized) {
    await AnimationsPanel.initialized;
  } else {
    await AnimationsPanel.once(AnimationsPanel.PANEL_INITIALIZED);
  }

  if (AnimationsController.animationPlayers.length !== 0) {
    await waitForAnimationTimelineRendering(AnimationsPanel);
    await waitForAllAnimationTargets(AnimationsPanel);
  }

  return {
    toolbox: toolbox,
    inspector: inspector,
    controller: AnimationsController,
    panel: AnimationsPanel,
    window: win
  };
};

/**
 * Close the toolbox.
 * @return a promise that resolves when the toolbox has closed.
 */
var closeAnimationInspector = async function() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);
};

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
var getAnimationPlayerState = async function(selector,
                                                    animationIndex = 0) {
  let playState = await executeInContent("Test:GetAnimationPlayerState",
                                         {selector, animationIndex});
  return playState;
};

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
 * This method should be called after "animation-timeline-rendering-completed" is emitted,
 * since we get all the AnimationTargetNode instances using getAnimationTargetNodes().
 * @param {AnimationsPanel} panel
 * @return {Array} all AnimationTargetNode instances
 */
var waitForAllAnimationTargets = async function(panel) {
  let targets = getAnimationTargetNodes(panel);
  await promise.all(targets.map(t => {
    if (!t.previewer.nodeFront) {
      return t.once("target-retrieved");
    }
    return false;
  }));
  return targets;
};

/**
 * Check the scrubber element in the timeline is moving.
 * @param {AnimationPanel} panel
 * @param {Boolean} isMoving
 */
async function assertScrubberMoving(panel, isMoving) {
  let timeline = panel.animationsTimelineComponent;

  if (isMoving) {
    // If we expect the scrubber to move, just wait for a couple of
    // timeline-data-changed events and compare times.
    let {time: time1} = await timeline.once("timeline-data-changed");
    let {time: time2} = await timeline.once("timeline-data-changed");
    ok(time2 > time1, "The scrubber is moving");
  } else {
    // If instead we expect the scrubber to remain at its position, just wait
    // for some time and make sure timeline-data-changed isn't emitted.
    let hasMoved = false;
    timeline.once("timeline-data-changed", () => {
      hasMoved = true;
    });
    await new Promise(r => setTimeout(r, 500));
    ok(!hasMoved, "The scrubber is not moving");
  }
}

/**
 * Click the play/pause button in the timeline toolbar and wait for animations
 * to update.
 * @param {AnimationsPanel} panel
 */
async function clickTimelinePlayPauseButton(panel) {
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);
  let onRendered = waitForAnimationTimelineRendering(panel);

  let btn = panel.playTimelineButtonEl;
  let win = btn.ownerDocument.defaultView;
  EventUtils.sendMouseEvent({type: "click"}, btn, win);

  await onUiUpdated;
  await onRendered;
  await waitForAllAnimationTargets(panel);
}

/**
 * Click the rewind button in the timeline toolbar and wait for animations to
 * update.
 * @param {AnimationsPanel} panel
 */
async function clickTimelineRewindButton(panel) {
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);
  let onRendered = waitForAnimationTimelineRendering(panel);

  let btn = panel.rewindTimelineButtonEl;
  let win = btn.ownerDocument.defaultView;
  EventUtils.sendMouseEvent({type: "click"}, btn, win);

  await onUiUpdated;
  await onRendered;
  await waitForAllAnimationTargets(panel);
}

/**
 * Select a rate inside the playback rate selector in the timeline toolbar and
 * wait for animations to update.
 * @param {AnimationsPanel} panel
 * @param {Number} rate The new rate value to be selected
 */
async function changeTimelinePlaybackRate(panel, rate) {
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

  await onUiUpdated;
  await waitForAnimationTimelineRendering(panel);
  await waitForAllAnimationTargets(panel);

  // Simulate a mousemove outside of the rate selector area to avoid subsequent
  // tests from failing because of unwanted mouseover events.
  EventUtils.synthesizeMouseAtCenter(
    win.document.querySelector("#timeline-toolbar"), {type: "mousemove"}, win);
}

/**
 * Wait for animation selecting.
 * @param {AnimationsPanel} panel
 */
async function waitForAnimationSelecting(panel) {
  await panel.animationsTimelineComponent.once("animation-selected");
}

/**
 * Wait for rendering animation timeline.
 * @param {AnimationsPanel} panel
 */
function waitForAnimationTimelineRendering(panel) {
  return panel.animationsTimelineComponent.once("animation-timeline-rendering-completed");
}

/**
   + * Click the timeline header to update the animation current time.
   + * @param {AnimationsPanel} panel
   + * @param {Number} x position rate on timeline header.
   + *                 This method calculates
   + *                 `position * offsetWidth + offsetLeft of timeline header`
   + *                 as the clientX of MouseEvent.
   + *                 This parameter should be from 0.0 to 1.0.
   + */
async function clickOnTimelineHeader(panel, position) {
  const timeline = panel.animationsTimelineComponent;
  const onTimelineDataChanged = timeline.once("timeline-data-changed");

  const header = timeline.timeHeaderEl;
  const clientX = header.offsetLeft + header.offsetWidth * position;
  EventUtils.sendMouseEvent({ type: "mousedown", clientX: clientX },
                            header, header.ownerDocument.defaultView);
  info(`Click at (${ clientX }, 0) on timeline header`);
  EventUtils.sendMouseEvent({ type: "mouseup", clientX: clientX }, header,
                            header.ownerDocument.defaultView);
  return onTimelineDataChanged;
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
 * @param {Boolean} shouldAlreadySelected Set to true
 *                  if the clicked animation is already selected.
 * @return {Promise} resolves to the animation whose state has changed.
 */
async function clickOnAnimation(panel, index, shouldAlreadySelected) {
  let timeline = panel.animationsTimelineComponent;

  // Expect a selection event.
  let onSelectionChanged = timeline.once(shouldAlreadySelected
                                         ? "animation-already-selected"
                                         : "animation-selected");

  info("Click on animation " + index + " in the timeline");
  let timeBlock = timeline.rootWrapperEl.querySelectorAll(".time-block")[index];
  // Scroll to show the timeBlock since the element may be out of displayed area.
  timeBlock.scrollIntoView(false);
  let timeBlockBounds = timeBlock.getBoundingClientRect();
  let x = timeBlockBounds.width / 2;
  let y = timeBlockBounds.height / 2;
  if (timeBlock != timeBlock.ownerDocument.elementFromPoint(x, y)) {
    // Move the mouse pointer a little since scrubber element may be at the point.
    x += timeBlockBounds.width / 4;
  }
  EventUtils.synthesizeMouse(timeBlock, x, y, {}, timeBlock.ownerDocument.defaultView);

  return onSelectionChanged;
}

/**
 * Get an instance of the Keyframes component from the timeline.
 * @param {AnimationsPanel} panel The panel instance.
 * @param {String} propertyName The name of the animated property.
 * @return {Keyframes} The Keyframes component instance.
 */
function getKeyframeComponent(panel, propertyName) {
  let timeline = panel.animationsTimelineComponent;
  let detailsComponent = timeline.details;
  return detailsComponent.keyframeComponents
                         .find(c => c.propertyName === propertyName);
}

/**
 * Get a keyframe element from the timeline.
 * @param {AnimationsPanel} panel The panel instance.
 * @param {String} propertyName The name of the animated property.
 * @param {Index} keyframeIndex The index of the keyframe.
 * @return {DOMNode} The keyframe element.
 */
function getKeyframeEl(panel, propertyName, keyframeIndex) {
  let keyframeComponent = getKeyframeComponent(panel, propertyName);
  return keyframeComponent.keyframesEl
                          .querySelectorAll(".frame")[keyframeIndex];
}

/**
 * Set style to test document.
 * @param {Animation} animation - animation object.
 * @param {AnimationsPanel} panel - The panel instance.
 * @param {String} name - property name.
 * @param {String} value - property value.
 * @param {String} selector - selector for test document.
 */
async function setStyle(animation, panel, name, value, selector) {
  info("Change the animation style via the content DOM. Setting " +
       name + " to " + value + " of " + selector);

  const onAnimationChanged = animation ? once(animation, "changed") : Promise.resolve();
  const onRendered = waitForAnimationTimelineRendering(panel);

  await executeInContent("devtools:test:setStyle", {
    selector: selector,
    propertyName: name,
    propertyValue: value
  });

  await onAnimationChanged;
  await onRendered;
  await waitForAllAnimationTargets(panel);
}

/**
 * Graph shapes of summary and detail are constructed by <path> element.
 * This function checks the vertex of path segments.
 * Also, if needed, checks the color for <stop> element.
 * @param pathEl - <path> element.
 * @param duration - float as duration which pathEl represetns.
 * @param hasClosePath - set true if the path shoud be closing.
 * @param expectedValues - JSON object format. We can test the vertex and color.
 *                         e.g.
 *                         [
 *                           // Test the vertex (x=0, y=0) should be passing through.
 *                           { x: 0, y: 0 },
 *                           { x: 0, y: 1 },
 *                           // If we have to test the color as well,
 *                           // we can write as following.
 *                           { x: 500, y: 1, color: "rgb(0, 0, 255)" },
 *                           { x: 1000, y: 1 }
 *                         ]
 */
function assertPathSegments(pathEl, duration, hasClosePath, expectedValues) {
  const pathSegList = pathEl.pathSegList;
  ok(pathSegList, "The tested element should have pathSegList");

  expectedValues.forEach(expectedValue => {
    ok(isPassingThrough(pathSegList, expectedValue.x, expectedValue.y),
       `The path segment of x ${ expectedValue.x }, y ${ expectedValue.y } `
       + `should be passing through`);

    if (expectedValue.color) {
      assertColor(pathEl.closest("svg"), expectedValue.x / duration, expectedValue.color);
    }
  });

  if (hasClosePath) {
    const closePathSeg = pathSegList.getItem(pathSegList.numberOfItems - 1);
    is(closePathSeg.pathSegType, closePathSeg.PATHSEG_CLOSEPATH,
       "The last segment should be close path");
  }
}

/**
 * Check the color for <stop> element.
 * @param svgEl - <svg> element which has <stop> element.
 * @param offset - float which represents the "offset" attribute of <stop>.
 * @param expectedColor - e.g. rgb(0, 0, 255)
 */
function assertColor(svgEl, offset, expectedColor) {
  const stopEl = findStopElement(svgEl, offset);
  ok(stopEl, `stop element at offset ${ offset } should exist`);
  is(stopEl.getAttribute("stop-color"), expectedColor,
     `stop-color of stop element at offset ${ offset } should be ${ expectedColor }`);
}

/**
 * Check whether the given vertex is passing throug on the path.
 * @param pathSegList - pathSegList of <path> element.
 * @param x - float x of vertex.
 * @param y - float y of vertex.
 * @return true: passing through, false: no on the path.
 */
function isPassingThrough(pathSegList, x, y) {
  let previousPathSeg = pathSegList.getItem(0);
  for (let i = 0; i < pathSegList.numberOfItems; i++) {
    const pathSeg = pathSegList.getItem(i);
    if (pathSeg.x === undefined) {
      continue;
    }
    const currentX = parseFloat(pathSeg.x.toFixed(3));
    const currentY = parseFloat(pathSeg.y.toFixed(6));
    if (currentX === x && currentY === y) {
      return true;
    }
    const previousX = parseFloat(previousPathSeg.x.toFixed(3));
    const previousY = parseFloat(previousPathSeg.y.toFixed(6));
    if (previousX <= x && x <= currentX &&
        Math.min(previousY, currentY) <= y && y <= Math.max(previousY, currentY)) {
      return true;
    }
    previousPathSeg = pathSeg;
  }
  return false;
}

/**
 * Find <stop> element which has given offset from given <svg> element.
 * @param svgEl - <svg> element which has <stop> element.
 * @param offset - float which represents the "offset" attribute of <stop>.
 * @return <stop> element.
 */
function findStopElement(svgEl, offset) {
  for (const stopEl of svgEl.querySelectorAll("stop")) {
    if (offset <= parseFloat(stopEl.getAttribute("offset"))) {
      return stopEl;
    }
  }

  return null;
}

/*
 * Returns all AnimationTargetNode instances.
 * This method should be called after emit "animation-timeline-rendering-completed".
 * @param {AnimationsPanel} panel The panel instance.
 * @return {Array} all AnimationTargetNode instances.
 */
function getAnimationTargetNodes(panel) {
  return panel.animationsTimelineComponent.animations.map(animation => {
    return panel.animationsTimelineComponent.componentsMap[animation.actorID].targetNode;
  });
}

/*
 * Returns all AnimationTargetBlock instances.
 * This method should be called after emit "animation-timeline-rendering-completed".
 * @param {AnimationsPanel} panel The panel instance.
 * @return {Array} all AnimationTargetBlock instances.
 */
function getAnimationTimeBlocks(panel) {
  return panel.animationsTimelineComponent.animations.map(animation => {
    return panel.animationsTimelineComponent.componentsMap[animation.actorID].timeBlock;
  });
}
