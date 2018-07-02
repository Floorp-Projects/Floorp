/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../test/head.js */
// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js", this);

const FRAME_SCRIPT_URL = CHROME_URL_ROOT + "doc_frame_script.js";
const TAB_NAME = "newanimationinspector";

const ANIMATION_L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// Enable new animation inspector.
Services.prefs.setBoolPref("devtools.new-animationinspector.enabled", true);

// Auto clean-up when a test ends.
// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.new-animationinspector.enabled");
  Services.prefs.clearUserPref("devtools.toolsidebar-width.inspector");
});

/**
 * Open the toolbox, with the inspector tool visible and the animationinspector
 * sidebar selected.
 *
 * @return {Promise} that resolves when the inspector is ready.
 */
const openAnimationInspector = async function() {
  const { inspector, toolbox } = await openInspectorSidebarTab(TAB_NAME);
  await inspector.once("inspector-updated");
  const { animationinspector: animationInspector } = inspector;
  await waitForRendering(animationInspector);
  const panel = inspector.panelWin.document.getElementById("animation-container");
  return { animationInspector, toolbox, inspector, panel };
};

/**
 * Close the toolbox.
 *
 * @return {Promise} that resolves when the toolbox has closed.
 */
const closeAnimationInspector = async function() {
  const target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.closeToolbox(target);
};

/**
 * Some animation features are not enabled by default in release/beta channels
 * yet including:
 *   * parts of the Web Animations API (Bug 1264101), and
 *   * the frames() timing function (Bug 1379582).
 */
const enableAnimationFeatures = function() {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.animations-api.core.enabled", true],
      ["layout.css.frames-timing.enabled", true],
    ]}, resolve);
  });
};

/**
 * Add a new test tab in the browser and load the given url.
 *
 * @param {String} url
 *        The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
const _addTab = addTab;
addTab = async function(url) {
  await enableAnimationFeatures();
  const tab = await _addTab(url);
  const browser = tab.linkedBrowser;
  info("Loading the helper frame script " + FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
  loadFrameScriptUtils(browser);
  return tab;
};

/**
 * Remove animated elements from document except given selectors.
 *
 * @param {Array} selectors
 * @return {Promise}
 */
const removeAnimatedElementsExcept = async function(selectors) {
  return executeInContent("Test:RemoveAnimatedElementsExcept", { selectors });
};

/**
 * Click on an animation in the timeline to select it.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} index
 *        The index of the animation to click on.
 */
const clickOnAnimation = async function(animationInspector, panel, index) {
  info("Click on animation " + index + " in the timeline");
  const summaryGraphEl = panel.querySelectorAll(".animation-summary-graph")[index];
  await clickOnSummaryGraph(animationInspector, panel, summaryGraphEl);
};

/**
 * Click on an animation by given selector of node which is target element of animation.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {String} selector
 *        Selector of node which is target element of animation.
 */
const clickOnAnimationByTargetSelector = async function(animationInspector,
                                                        panel, selector) {
  info(`Click on animation whose selector of target element is '${ selector }'`);
  const animationItemEl = findAnimationItemElementsByTargetSelector(panel, selector);
  const summaryGraphEl = animationItemEl.querySelector(".animation-summary-graph");
  await clickOnSummaryGraph(animationInspector, panel, summaryGraphEl);
};

/**
 * Click on close button for animation detail pane.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 */
const clickOnDetailCloseButton = function(panel) {
  info("Click on close button for animation detail pane");
  const buttonEl = panel.querySelector(".animation-detail-close-button");
  const bounds = buttonEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(buttonEl, x, y, {}, buttonEl.ownerGlobal);
};

/**
 * Click on pause/resume button.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
const clickOnPauseResumeButton = async function(animationInspector, panel) {
  info("Click on pause/resume button");
  const buttonEl = panel.querySelector(".pause-resume-button");
  const bounds = buttonEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(buttonEl, x, y, {}, buttonEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Click on rewind button.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
const clickOnRewindButton = async function(animationInspector, panel) {
  info("Click on rewind button");
  const buttonEl = panel.querySelector(".rewind-button");
  const bounds = buttonEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(buttonEl, x, y, {}, buttonEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Click on the scrubber controller pane to update the animation current time.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} mouseDownPosition
 *        rate on scrubber controller pane.
 *        This method calculates
 *        `mouseDownPosition * offsetWidth + offsetLeft of scrubber controller pane`
 *        as the clientX of MouseEvent.
 */
const clickOnCurrentTimeScrubberController = async function(animationInspector,
                                                            panel,
                                                            mouseDownPosition,
                                                            mouseMovePosition) {
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  const bounds = controllerEl.getBoundingClientRect();
  const mousedonwX = bounds.width * mouseDownPosition;

  info(`Click ${ mousedonwX } on scrubber controller`);
  EventUtils.synthesizeMouse(controllerEl, mousedonwX, 0, {}, controllerEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Click on the inspect icon for the given AnimationTargetComponent.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} index
 *        The index of the AnimationTargetComponent to click on.
 */
const clickOnInspectIcon = async function(animationInspector, panel, index) {
  info(`Click on an inspect icon in animation target component[${ index }]`);
  const iconEl =
    panel.querySelectorAll(".animation-target .objectBox .open-inspector")[index];
  iconEl.scrollIntoView(false);
  EventUtils.synthesizeMouseAtCenter(iconEl, {}, iconEl.ownerGlobal);
  // We wait just one time, because the components are updated synchronously.
  await animationInspector.once("animation-target-rendered");
};

/**
 * Click on playback rate selector to select given rate.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} rate
 */
const clickOnPlaybackRateSelector = async function(animationInspector, panel, rate) {
  info(`Click on playback rate selector to select ${rate}`);
  const selectEl = panel.querySelector(".playback-rate-selector");
  const optionEl = [...selectEl.options].filter(o => Number(o.value) === rate)[0];

  if (!optionEl) {
    ok(false, `Could not find an option for rate ${ rate } in the rate selector. ` +
              `Values are: ${ [...selectEl.options].map(o => o.value) }`);
    return;
  }

  const win = selectEl.ownerGlobal;
  EventUtils.synthesizeMouseAtCenter(selectEl, { type: "mousedown" }, win);
  EventUtils.synthesizeMouseAtCenter(optionEl, { type: "mouseup" }, win);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Click on given summary graph element.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Element} summaryGraphEl
 */
const clickOnSummaryGraph = async function(animationInspector, panel, summaryGraphEl) {
  // Disable pointer-events of the scrubber in order to avoid to click accidently.
  const scrubberEl = panel.querySelector(".current-time-scrubber");
  scrubberEl.style.pointerEvents = "none";
  // Scroll to show the timeBlock since the element may be out of displayed area.
  summaryGraphEl.scrollIntoView(false);
  EventUtils.synthesizeMouseAtCenter(summaryGraphEl, {}, summaryGraphEl.ownerGlobal);
  await waitForAnimationDetail(animationInspector);
  // Restore the scrubber style.
  scrubberEl.style.pointerEvents = "unset";
};

/**
 * Click on the target node for the given AnimationTargetComponent index.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} index
 *        The index of the AnimationTargetComponent to click on.
 */
const clickOnTargetNode = async function(animationInspector, panel, index) {
  info(`Click on a target node in animation target component[${ index }]`);
  const targetEl = panel.querySelectorAll(".animation-target .objectBox")[index];
  targetEl.scrollIntoView(false);
  const onHighlight = animationInspector.inspector.toolbox.once("node-highlight");
  const onAnimationTargetUpdated = animationInspector.once("animation-target-rendered");
  EventUtils.synthesizeMouseAtCenter(targetEl, {}, targetEl.ownerGlobal);
  await onAnimationTargetUpdated;
  await waitForSummaryAndDetail(animationInspector);
  await onHighlight;
};

/**
 * Drag on the scrubber to update the animation current time.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} mouseDownPosition
 *        rate on scrubber controller pane.
 *        This method calculates
 *        `mouseDownPosition * offsetWidth + offsetLeft of scrubber controller pane`
 *        as the clientX of MouseEvent.
 * @param {Number} mouseMovePosition
 *        Dispatch mousemove event with mouseMovePosition after mousedown.
 *        Calculation for clinetX is same to above.
 * @param {Number} mouseYPixel
 *        Y of mouse in pixel.
 */
const dragOnCurrentTimeScrubber = async function(animationInspector,
                                                 panel,
                                                 mouseDownPosition,
                                                 mouseMovePosition,
                                                 mouseYPixel) {
  const controllerEl = panel.querySelector(".current-time-scrubber");
  const bounds = controllerEl.getBoundingClientRect();
  const mousedonwX = bounds.width * mouseDownPosition;
  const mousemoveX = bounds.width * mouseMovePosition;

  info(`Drag on scrubber from ${ mousedonwX } to ${ mousemoveX }`);
  EventUtils.synthesizeMouse(controllerEl, mousedonwX, mouseYPixel,
                             { type: "mousedown" }, controllerEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
  EventUtils.synthesizeMouse(controllerEl, mousemoveX, mouseYPixel,
                             { type: "mousemove" }, controllerEl.ownerGlobal);
  EventUtils.synthesizeMouse(controllerEl, mousemoveX, mouseYPixel,
                             { type: "mouseup" }, controllerEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Drag on the scrubber controller pane to update the animation current time.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} mouseDownPosition
 *        rate on scrubber controller pane.
 *        This method calculates
 *        `mouseDownPosition * offsetWidth + offsetLeft of scrubber controller pane`
 *        as the clientX of MouseEvent.
 * @param {Number} mouseMovePosition
 *        Dispatch mousemove event with mouseMovePosition after mousedown.
 *        Calculation for clinetX is same to above.
 */
const dragOnCurrentTimeScrubberController = async function(animationInspector,
                                                            panel,
                                                            mouseDownPosition,
                                                            mouseMovePosition) {
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  const bounds = controllerEl.getBoundingClientRect();
  const mousedonwX = bounds.width * mouseDownPosition;
  const mousemoveX = bounds.width * mouseMovePosition;

  info(`Drag on scrubber controller from ${ mousedonwX } to ${ mousemoveX }`);
  EventUtils.synthesizeMouse(controllerEl, mousedonwX, 0,
                             { type: "mousedown" }, controllerEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
  EventUtils.synthesizeMouse(controllerEl, mousemoveX, 0,
                             { type: "mousemove" }, controllerEl.ownerGlobal);
  EventUtils.synthesizeMouse(controllerEl, mousemoveX, 0,
                             { type: "mouseup" }, controllerEl.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Get current animation duration and rate of
 * clickOrDragOnCurrentTimeScrubberController in given pixels.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} pixels
 * @return {Object}
 *         {
 *           duration,
 *           rate,
 *         }
 */
const getDurationAndRate = function(animationInspector, panel, pixels) {
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  const bounds = controllerEl.getBoundingClientRect();
  const duration =
    animationInspector.state.timeScale.getDuration() / bounds.width * pixels;
  const rate = 1 / bounds.width * pixels;
  return { duration, rate };
};

/**
 * Mouse over the target node for the given AnimationTargetComponent index.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} index
 *        The index of the AnimationTargetComponent to click on.
 */
const mouseOverOnTargetNode = function(animationInspector, panel, index) {
  info(`Mouse over on a target node in animation target component[${ index }]`);
  const el = panel.querySelectorAll(".animation-target .objectBox")[index];
  el.scrollIntoView(false);
  EventUtils.synthesizeMouse(el, 10, 5, { type: "mouseover" }, el.ownerGlobal);
};

/**
 * Mouse out of the target node for the given AnimationTargetComponent index.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} index
 *        The index of the AnimationTargetComponent to click on.
 */
const mouseOutOnTargetNode = function(animationInspector, panel, index) {
  info(`Mouse out on a target node in animation target component[${ index }]`);
  const el = panel.querySelectorAll(".animation-target .objectBox")[index];
  el.scrollIntoView(false);
  EventUtils.synthesizeMouse(el, -1, -1, { type: "mouseout" }, el.ownerGlobal);
};

/**
 * Select animation inspector in sidebar and toolbar.
 *
 * @param {InspectorPanel} inspector
 */
const selectAnimationInspector = async function(inspector) {
  await inspector.toolbox.selectTool("inspector");
  const onUpdated = inspector.once("inspector-updated");
  inspector.sidebar.select("newanimationinspector");
  await onUpdated;
  await waitForRendering(inspector.animationinspector);
};

/**
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector and wait for the animations to be displayed
 *
 * @param {String|NodeFront}
 *        data The node to select
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not
 *        to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 *                   and animations of its subtree are properly displayed.
 */
const selectNodeAndWaitForAnimations = async function(data, inspector, reason = "test") {
  // We want to make sure the rest of the test waits for the animations to
  // be properly displayed (wait for all target DOM nodes to be previewed).
  const onUpdated = inspector.once("inspector-updated");
  await selectNode(data, inspector, reason);
  await onUpdated;
  await waitForRendering(inspector.animationinspector);
};

/**
 * Send keyboard event of space to given panel.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
const sendSpaceKeyEvent = async function(animationInspector, panel) {
  panel.focus();
  EventUtils.sendKey("SPACE", panel.ownerGlobal);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Set a node class attribute to the given selector.
 *
 * @param {AnimationInspector} animationInspector
 * @param {String} selector
 * @param {String} cls
 *        e.g. ".ball.still"
 */
const setClassAttribute = async function(animationInspector, selector, cls) {
  const options = {
    attributeName: "class",
    attributeValue: cls,
    selector,
  };
  await executeInContent("devtools:test:setAttribute", options);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Set the sidebar width by given parameter.
 *
 * @param {String} width
 *        Change sidebar width by given parameter.
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return {Promise} Resolves when the sidebar size changed.
 */
const setSidebarWidth = async function(width, inspector) {
  const onUpdated = inspector.toolbox.once("inspector-sidebar-resized");
  inspector.splitBox.setState({ width });
  await onUpdated;
};

/**
 * Set a new style property declaration to the node for the given selector.
 *
 * @param {AnimationInspector} animationInspector
 * @param {String} selector
 * @param {String} propertyName
 *        e.g. "animationDuration"
 * @param {String} propertyValue
 *        e.g. "5.5s"
 */
const setStyle = async function(animationInspector,
                                selector, propertyName, propertyValue) {
  const options = {
    propertyName,
    propertyValue,
    selector,
  };
  await executeInContent("devtools:test:setStyle", options);
  await waitForSummaryAndDetail(animationInspector);
};

/**
 * Wait for rendering.
 *
 * @param {AnimationInspector} animationInspector
 */
const waitForRendering = async function(animationInspector) {
  await Promise.all([
    waitForAllAnimationTargets(animationInspector),
    waitForAllSummaryGraph(animationInspector),
    waitForAnimationDetail(animationInspector),
  ]);
};

/**
 * Wait for rendering of animation keyframes.
 *
 * @param {AnimationInspector} inspector
 */

const waitForAnimationDetail = async function(animationInspector) {
  if (animationInspector.state.selectedAnimation &&
      animationInspector.state.detailVisibility) {
    await animationInspector.once("animation-keyframes-rendered");
  }
};

/**
 * Wait for all AnimationTarget components to be fully loaded
 * (fetched their related actor and rendered).
 *
 * @param {AnimationInspector} animationInspector
 */
const waitForAllAnimationTargets = async function(animationInspector) {
  const panel =
    animationInspector.inspector.panelWin.document.getElementById("animation-container");
  const objectBoxCount = panel.querySelectorAll(".animation-target .objectBox").length;

  if (objectBoxCount === animationInspector.state.animations.length) {
    return;
  }

  for (let i = 0; i < animationInspector.state.animations.length - objectBoxCount; i++) {
    await animationInspector.once("animation-target-rendered");
  }
};

/**
 * Wait for all SummaryGraph components to be fully loaded
 *
 * @param {AnimationInspector} inspector
 */
const waitForAllSummaryGraph = async function(animationInspector) {
  for (let i = 0; i < animationInspector.state.animations.length; i++) {
    await animationInspector.once("animation-summary-graph-rendered");
  }
};

/**
 * Wait for rendering of all summary graph and detail.
 *
 * @param {AnimationInspector} inspector
 */
const waitForSummaryAndDetail = async function(animationInspector) {
  await Promise.all([
    waitForAllSummaryGraph(animationInspector),
    waitForAnimationDetail(animationInspector),
  ]);
};

/**
 * Check whether current time of all animations and UI are given specified time.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} time
 */
function assertAnimationsCurrentTime(animationInspector, time) {
  const isTimeEqual =
    animationInspector.state.animations.every(({state}) => state.currentTime === time);
  ok(isTimeEqual, `Current time of animations should be ${ time }`);
}

/**
 * Check whether the animations are pausing.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
function assertAnimationsPausing(animationInspector, panel) {
  assertAnimationsPausingOrRunning(animationInspector, panel, true);
}

/**
 * Check whether the animations are pausing/running.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {boolean} shouldPause
 */
function assertAnimationsPausingOrRunning(animationInspector, panel, shouldPause) {
  const hasRunningAnimation =
    animationInspector.state.animations.some(({state}) => state.playState === "running");

  if (shouldPause) {
    is(hasRunningAnimation, false, "All animations should be paused");
  } else {
    is(hasRunningAnimation, true, "Animations should be running at least one");
  }
}

/**
 * Check whether the animations are running.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
function assertAnimationsRunning(animationInspector, panel) {
  assertAnimationsPausingOrRunning(animationInspector, panel, false);
}

/**
 * Check the <stop> element in the given linearGradientEl for the correct offset
 * and color attributes.
 *
 * @param {Element} linearGradientEl
          <linearGradient> element which has <stop> element.
 * @param {Number} offset
 *        float which represents the "offset" attribute of <stop>.
 * @param {String} expectedColor
 *        e.g. rgb(0, 0, 255)
 */
function assertLinearGradient(linearGradientEl, offset, expectedColor) {
  const stopEl = findStopElement(linearGradientEl, offset);
  ok(stopEl, `stop element at offset ${ offset } should exist`);
  is(stopEl.getAttribute("stop-color"), expectedColor,
    `stop-color of stop element at offset ${ offset } should be ${ expectedColor }`);
}

/**
 * SummaryGraph is constructed by <path> element.
 * This function checks the vertex of path segments.
 *
 * @param {Element} pathEl
 *        <path> element.
 * @param {boolean} hasClosePath
 *        Set true if the path shoud be closing.
 * @param {Object} expectedValues
 *        JSON object format. We can test the vertex and color.
 *        e.g.
 *        [
 *          { x: 0, y: 0 },
 *          { x: 0, y: 1 },
 *        ]
 */
function assertPathSegments(pathEl, hasClosePath, expectedValues) {
  const pathSegList = pathEl.pathSegList;
  ok(pathSegList, "The tested element should have pathSegList");

  expectedValues.forEach(expectedValue => {
    ok(isPassingThrough(pathSegList, expectedValue.x, expectedValue.y),
       `The path segment of x ${ expectedValue.x }, y ${ expectedValue.y } `
       + `should be passing through`);
  });

  if (hasClosePath) {
    const closePathSeg = pathSegList.getItem(pathSegList.numberOfItems - 1);
    is(closePathSeg.pathSegType, closePathSeg.PATHSEG_CLOSEPATH,
       "The last segment should be close path");
  }
}

/**
 * Check whether the given vertex is passing throug on the path.
 *
 * @param {pathSegList} pathSegList - pathSegList of <path> element.
 * @param {float} x - x of vertex.
 * @param {float} y - y of vertex.
 * @return {boolean} true: passing through, false: no on the path.
 */
function isPassingThrough(pathSegList, x, y) {
  let previousPathSeg = pathSegList.getItem(0);
  for (let i = 0; i < pathSegList.numberOfItems; i++) {
    const pathSeg = pathSegList.getItem(i);
    if (pathSeg.x === undefined) {
      continue;
    }
    const currentX = parseFloat(pathSeg.x.toFixed(3));
    const currentY = parseFloat(pathSeg.y.toFixed(3));
    if (currentX === x && currentY === y) {
      return true;
    }
    const previousX = parseFloat(previousPathSeg.x.toFixed(3));
    const previousY = parseFloat(previousPathSeg.y.toFixed(3));
    if (previousX <= x && x <= currentX &&
        Math.min(previousY, currentY) <= y && y <= Math.max(previousY, currentY)) {
      return true;
    }
    previousPathSeg = pathSeg;
  }
  return false;
}

/**
 * Return animation item element by target node selector.
 * This function compares betweem animation-target textContent and given selector.
 * Then returns matched first item.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {String} selector
 *        Selector of tested element.
 * @return {DOMElement}
 *        Animation item element.
 */
function findAnimationItemElementsByTargetSelector(panel, selector) {
  const attrNameEls = panel.querySelectorAll(".animation-target .attrName");
  const regexp = new RegExp(`\\${ selector }(\\.|$)`, "gi");

  for (const attrNameEl of attrNameEls) {
    if (regexp.exec(attrNameEl.textContent)) {
      return attrNameEl.closest(".animation-item");
    }
  }

  return null;
}

/**
 * Find the <stop> element which has the given offset in the given linearGradientEl.
 *
 * @param {Element} linearGradientEl
 *        <linearGradient> element which has <stop> element.
 * @param {Number} offset
 *        Float which represents the "offset" attribute of <stop>.
 * @return {Element}
 *         If can't find suitable element, returns null.
 */
function findStopElement(linearGradientEl, offset) {
  for (const stopEl of linearGradientEl.querySelectorAll("stop")) {
    if (offset <= parseFloat(stopEl.getAttribute("offset"))) {
      return stopEl;
    }
  }

  return null;
}

/**
 * Do test for keyframes-graph_computed-value-path-1/2.
 *
 * @param {Array} testData
 */
async function testKeyframesGraphComputedValuePath(testData) {
  await addTab(URL_ROOT + "doc_multi_keyframes.html");
  await removeAnimatedElementsExcept(testData.map(t => `.${ t.targetClass }`));
  const { animationInspector, panel } = await openAnimationInspector();

  for (const { properties, targetClass } of testData) {
    info(`Checking keyframes graph for ${ targetClass }`);
    await clickOnAnimationByTargetSelector(animationInspector,
                                           panel, `.${ targetClass }`);

    for (const property of properties) {
      const {
        name,
        computedValuePathClass,
        expectedPathSegments,
        expectedStopColors,
      } = property;

      const testTarget = `${ name } in ${ targetClass }`;
      info(`Checking keyframes graph for ${ testTarget }`);
      info(`Checking keyframes graph path existence for ${ testTarget }`);
      const keyframesGraphPathEl = panel.querySelector(`.${ name }`);
      ok(keyframesGraphPathEl,
         `The keyframes graph path element of ${ testTarget } should be existence`);

      info(`Checking computed value path existence for ${ testTarget }`);
      const computedValuePathEl =
        keyframesGraphPathEl.querySelector(`.${ computedValuePathClass }`);
      ok(computedValuePathEl,
         `The computed value path element of ${ testTarget } should be existence`);

      info(`Checking path segments for ${ testTarget }`);
      const pathEl = computedValuePathEl.querySelector("path");
      ok(pathEl, `The <path> element of ${ testTarget } should be existence`);
      assertPathSegments(pathEl, true, expectedPathSegments);

      if (!expectedStopColors) {
        continue;
      }

      info(`Checking linearGradient for ${ testTarget }`);
      const linearGradientEl = computedValuePathEl.querySelector("linearGradient");
      ok(linearGradientEl,
         `The <linearGradientEl> element of ${ testTarget } should be existence`);

      for (const expectedStopColor of expectedStopColors) {
        const { offset, color } = expectedStopColor;
        assertLinearGradient(linearGradientEl, offset, color);
      }
    }
  }
}

/**
 * Check the adjusted current time and created time from specified two animations.
 *
 * @param {AnimationPlayerFront.state} animation1
 * @param {AnimationPlayerFront.state} animation2
 */
function checkAdjustingTheTime(animation1, animation2) {
  const adjustedCurrentTimeDiff = animation2.currentTime / animation2.playbackRate
                                  - animation1.currentTime / animation1.playbackRate;
  const createdTimeDiff = animation1.createdTime - animation2.createdTime;
  ok(Math.abs(adjustedCurrentTimeDiff - createdTimeDiff) < 0.1,
     "Adjusted time is correct");
}
