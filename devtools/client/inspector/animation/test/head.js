/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/* import-globals-from ../../test/head.js */
// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js", this);

const COMMON_FRAME_SCRIPT_URL = "chrome://devtools/content/shared/frame-script-utils.js";
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
const openAnimationInspector = async function () {
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
const closeAnimationInspector = async function () {
  const target = TargetFactory.forTab(gBrowser.selectedTab);
  return gDevTools.closeToolbox(target);
};

/**
 * Some animation features are not enabled by default in release/beta channels
 * yet including:
 *   * parts of the Web Animations API (Bug 1264101), and
 *   * the frames() timing function (Bug 1379582).
 */
const enableAnimationFeatures = function () {
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
addTab = async function (url) {
  await enableAnimationFeatures();
  const tab = await _addTab(url);
  const browser = tab.linkedBrowser;
  info("Loading the helper frame script " + FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
  info("Loading the helper frame script " + COMMON_FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(COMMON_FRAME_SCRIPT_URL, false);
  return tab;
};

/**
 * Click on an animation in the timeline to select it.
 *
 * @param {AnimationInspector} animationInspector.
 * @param {AnimationsPanel} panel
 *        The panel instance.
 * @param {Number} index
 *        The index of the animation to click on.
 */
const clickOnAnimation = async function (animationInspector, panel, index) {
  info("Click on animation " + index + " in the timeline");
  const summaryGraphEl = panel.querySelectorAll(".animation-summary-graph")[index];
  // Scroll to show the timeBlock since the element may be out of displayed area.
  summaryGraphEl.scrollIntoView(false);
  const bounds = summaryGraphEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(summaryGraphEl, x, y, {}, summaryGraphEl.ownerGlobal);

  await waitForAnimationDetail(animationInspector);
};

/**
 * Click on close button for animation detail pane.
 *
 * @param {AnimationsPanel} panel
 *        The panel instance.
 */
const clickOnDetailCloseButton = function (panel) {
  info("Click on close button for animation detail pane");
  const buttonEl = panel.querySelector(".animation-detail-close-button");
  const bounds = buttonEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(buttonEl, x, y, {}, buttonEl.ownerGlobal);
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
const selectNodeAndWaitForAnimations = async function (data, inspector, reason = "test") {
  // We want to make sure the rest of the test waits for the animations to
  // be properly displayed (wait for all target DOM nodes to be previewed).
  const onUpdated = inspector.once("inspector-updated");
  await selectNode(data, inspector, reason);
  await onUpdated;
  await waitForRendering(inspector.animationinspector);
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
const setSidebarWidth = async function (width, inspector) {
  const onUpdated = inspector.toolbox.once("inspector-sidebar-resized");
  inspector.splitBox.setState({ width });
  await onUpdated;
};

/**
 * Wait for rendering.
 *
 * @param {AnimationInspector} animationInspector
 */
const waitForRendering = async function (animationInspector) {
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
const waitForAnimationDetail = async function (animationInspector) {
  if (animationInspector.animations.length === 1) {
    await animationInspector.once("animation-keyframes-rendered");
  }
};

/**
 * Wait for all AnimationTarget components to be fully loaded
 * (fetched their related actor and rendered).
 *
 * @param {AnimationInspector} animationInspector
 */
const waitForAllAnimationTargets = async function (animationInspector) {
  for (let i = 0; i < animationInspector.animations.length; i++) {
    await animationInspector.once("animation-target-rendered");
  }
};

/**
 * Wait for all SummaryGraph components to be fully loaded
 *
 * @param {AnimationInspector} inspector
 */
const waitForAllSummaryGraph = async function (animationInspector) {
  for (let i = 0; i < animationInspector.animations.length; i++) {
    await animationInspector.once("animation-summary-graph-rendered");
  }
};

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
 * Return animation item element by target node class.
 * This function compares betweem animation-target textContent and given className.
 * Also, this function premises one class name.
 *
 * @param {Element} panel - root element of animation inspector.
 * @param {String} targetClassName - class name of tested element.
 * @return {Element} animation item element.
 */
function findAnimationItemElementsByTargetClassName(panel, targetClassName) {
  const animationTargetEls = panel.querySelectorAll(".animation-target");

  for (const animationTargetEl of animationTargetEls) {
    const className = animationTargetEl.textContent.split(".")[1];

    if (className === targetClassName) {
      return animationTargetEl.closest(".animation-item");
    }
  }

  return null;
}
