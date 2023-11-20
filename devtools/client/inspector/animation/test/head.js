/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

const TAB_NAME = "animationinspector";

const ANIMATION_L10N = new LocalizationHelper(
  "devtools/client/locales/animationinspector.properties"
);

// Auto clean-up when a test ends.
// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
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
  const animationInspector = inspector.getPanel("animationinspector");
  const panel = inspector.panelWin.document.getElementById(
    "animation-container"
  );

  info("Wait for loading first content");
  const count = getDisplayedGraphCount(animationInspector, panel);
  await waitUntil(
    () =>
      panel.querySelectorAll(".animation-summary-graph-path").length >= count &&
      panel.querySelectorAll(".animation-target .objectBox").length >= count
  );

  if (
    animationInspector.state.selectedAnimation &&
    animationInspector.state.detailVisibility
  ) {
    await waitUntil(() => panel.querySelector(".animated-property-list"));
  }

  return { animationInspector, toolbox, inspector, panel };
};

/**
 * Close the toolbox.
 *
 * @return {Promise} that resolves when the toolbox has closed.
 */
const closeAnimationInspector = async function () {
  return gDevTools.closeToolboxForTab(gBrowser.selectedTab);
};

/**
 * Some animation features are not enabled by default in release/beta channels
 * yet including parts of the Web Animations API.
 */
const enableAnimationFeatures = function () {
  return SpecialPowers.pushPrefEnv({
    set: [
      ["dom.animations-api.getAnimations.enabled", true],
      ["dom.animations-api.timelines.enabled", true],
    ],
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
  return _addTab(url);
};

/**
 * Remove animated elements from document except given selectors.
 *
 * @param {Array} selectors
 * @return {Promise}
 */
const removeAnimatedElementsExcept = function (selectors) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selectors],
    selectorsChild => {
      function isRemovableElement(animation, selectorsInner) {
        for (const selector of selectorsInner) {
          if (animation.effect.target.matches(selector)) {
            return false;
          }
        }

        return true;
      }

      for (const animation of content.document.getAnimations()) {
        if (isRemovableElement(animation, selectorsChild)) {
          animation.effect.target.remove();
        }
      }
    }
  );
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
const clickOnAnimation = async function (animationInspector, panel, index) {
  info("Click on animation " + index + " in the timeline");
  const animationItemEl = await findAnimationItemByIndex(panel, index);
  const summaryGraphEl = animationItemEl.querySelector(
    ".animation-summary-graph"
  );
  clickOnSummaryGraph(animationInspector, panel, summaryGraphEl);
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
const clickOnAnimationByTargetSelector = async function (
  animationInspector,
  panel,
  selector
) {
  info(`Click on animation whose selector of target element is '${selector}'`);
  const animationItemEl = await findAnimationItemByTargetSelector(
    panel,
    selector
  );
  const summaryGraphEl = animationItemEl.querySelector(
    ".animation-summary-graph"
  );
  clickOnSummaryGraph(animationInspector, panel, summaryGraphEl);
};

/**
 * Click on close button for animation detail pane.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
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
 * Click on pause/resume button.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
const clickOnPauseResumeButton = function (animationInspector, panel) {
  info("Click on pause/resume button");
  const buttonEl = panel.querySelector(".pause-resume-button");
  const bounds = buttonEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(buttonEl, x, y, {}, buttonEl.ownerGlobal);
};

/**
 * Click on rewind button.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 */
const clickOnRewindButton = function (animationInspector, panel) {
  info("Click on rewind button");
  const buttonEl = panel.querySelector(".rewind-button");
  const bounds = buttonEl.getBoundingClientRect();
  const x = bounds.width / 2;
  const y = bounds.height / 2;
  EventUtils.synthesizeMouse(buttonEl, x, y, {}, buttonEl.ownerGlobal);
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
const clickOnCurrentTimeScrubberController = function (
  animationInspector,
  panel,
  mouseDownPosition
) {
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  const bounds = controllerEl.getBoundingClientRect();
  const mousedonwX = bounds.width * mouseDownPosition;

  info(`Click ${mousedonwX} on scrubber controller`);
  EventUtils.synthesizeMouse(
    controllerEl,
    mousedonwX,
    0,
    {},
    controllerEl.ownerGlobal
  );
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
const clickOnInspectIcon = async function (animationInspector, panel, index) {
  info(`Click on an inspect icon in animation target component[${index}]`);
  const animationItemEl = await findAnimationItemByIndex(panel, index);
  const iconEl = animationItemEl.querySelector(
    ".animation-target .objectBox .highlight-node"
  );
  iconEl.scrollIntoView(false);
  EventUtils.synthesizeMouseAtCenter(iconEl, {}, iconEl.ownerGlobal);
};

/**
 * Change playback rate selector to select given rate.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} rate
 */
const changePlaybackRateSelector = async function (
  animationInspector,
  panel,
  rate
) {
  info(`Click on playback rate selector to select ${rate}`);
  const selectEl = panel.querySelector(".playback-rate-selector");
  const optionIndex = [...selectEl.options].findIndex(o => +o.value == rate);

  if (optionIndex == -1) {
    ok(
      false,
      `Could not find an option for rate ${rate} in the rate selector. ` +
        `Values are: ${[...selectEl.options].map(o => o.value)}`
    );
    return;
  }

  selectEl.focus();

  const win = selectEl.ownerGlobal;
  while (selectEl.selectedIndex != optionIndex) {
    const key = selectEl.selectedIndex > optionIndex ? "LEFT" : "RIGHT";
    EventUtils.sendKey(key, win);
  }
};

/**
 * Click on given summary graph element.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Element} summaryGraphEl
 */
const clickOnSummaryGraph = function (
  animationInspector,
  panel,
  summaryGraphEl
) {
  // Disable pointer-events of the scrubber in order to avoid to click accidently.
  const scrubberEl = panel.querySelector(".current-time-scrubber");
  scrubberEl.style.pointerEvents = "none";
  // Scroll to show the timeBlock since the element may be out of displayed area.
  summaryGraphEl.scrollIntoView(false);
  EventUtils.synthesizeMouseAtCenter(
    summaryGraphEl,
    {},
    summaryGraphEl.ownerGlobal
  );
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
const clickOnTargetNode = async function (animationInspector, panel, index) {
  const { inspector } = animationInspector;
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);
  info(`Click on a target node in animation target component[${index}]`);

  const animationItemEl = await findAnimationItemByIndex(panel, index);
  const targetEl = animationItemEl.querySelector(
    ".animation-target .objectBox"
  );
  const onHighlight = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  EventUtils.synthesizeMouseAtCenter(targetEl, {}, targetEl.ownerGlobal);
  await onHighlight;
};

/**
 * Drag on the scrubber to update the animation current time.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} mouseMovePixel
 *        Dispatch mousemove event with mouseMovePosition after mousedown.
 * @param {Number} mouseYPixel
 *        Y of mouse in pixel.
 */
const dragOnCurrentTimeScrubber = async function (
  animationInspector,
  panel,
  mouseMovePixel,
  mouseYPixel
) {
  const controllerEl = panel.querySelector(".current-time-scrubber");
  info(`Drag scrubber to X ${mouseMovePixel}`);
  EventUtils.synthesizeMouse(
    controllerEl,
    0,
    mouseYPixel,
    { type: "mousedown" },
    controllerEl.ownerGlobal
  );
  await waitUntilAnimationsPlayState(animationInspector, "paused");

  const animation = animationInspector.state.animations[0];
  let currentTime = animation.state.currentTime;
  EventUtils.synthesizeMouse(
    controllerEl,
    mouseMovePixel,
    mouseYPixel,
    { type: "mousemove" },
    controllerEl.ownerGlobal
  );
  await waitUntil(() => animation.state.currentTime !== currentTime);

  currentTime = animation.state.currentTime;
  EventUtils.synthesizeMouse(
    controllerEl,
    mouseMovePixel,
    mouseYPixel,
    { type: "mouseup" },
    controllerEl.ownerGlobal
  );
  await waitUntil(() => animation.state.currentTime !== currentTime);
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
const dragOnCurrentTimeScrubberController = async function (
  animationInspector,
  panel,
  mouseDownPosition,
  mouseMovePosition
) {
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  const bounds = controllerEl.getBoundingClientRect();
  const mousedonwX = bounds.width * mouseDownPosition;
  const mousemoveX = bounds.width * mouseMovePosition;

  info(`Drag on scrubber controller from ${mousedonwX} to ${mousemoveX}`);
  EventUtils.synthesizeMouse(
    controllerEl,
    mousedonwX,
    0,
    { type: "mousedown" },
    controllerEl.ownerGlobal
  );
  await waitUntilAnimationsPlayState(animationInspector, "paused");

  const animation = animationInspector.state.animations[0];
  let currentTime = animation.state.currentTime;
  EventUtils.synthesizeMouse(
    controllerEl,
    mousemoveX,
    0,
    { type: "mousemove" },
    controllerEl.ownerGlobal
  );
  await waitUntil(() => animation.state.currentTime !== currentTime);

  currentTime = animation.state.currentTime;
  EventUtils.synthesizeMouse(
    controllerEl,
    mousemoveX,
    0,
    { type: "mouseup" },
    controllerEl.ownerGlobal
  );
  await waitUntil(() => animation.state.currentTime !== currentTime);
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
const getDurationAndRate = function (animationInspector, panel, pixels) {
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  const bounds = controllerEl.getBoundingClientRect();
  const duration =
    (animationInspector.state.timeScale.getDuration() / bounds.width) * pixels;
  const rate = (1 / bounds.width) * pixels;
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
const mouseOverOnTargetNode = function (animationInspector, panel, index) {
  info(`Mouse over on a target node in animation target component[${index}]`);
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
const mouseOutOnTargetNode = function (animationInspector, panel, index) {
  info(`Mouse out on a target node in animation target component[${index}]`);
  const el = panel.querySelectorAll(".animation-target .objectBox")[index];
  el.scrollIntoView(false);
  EventUtils.synthesizeMouse(el, -1, -1, { type: "mouseout" }, el.ownerGlobal);
};

/**
 * Select animation inspector in sidebar and toolbar.
 *
 * @param {InspectorPanel} inspector
 */
const selectAnimationInspector = async function (inspector) {
  await inspector.toolbox.selectTool("inspector");
  const onDispatched = waitForDispatch(inspector.store, "UPDATE_ANIMATIONS");
  inspector.sidebar.select("animationinspector");
  await onDispatched;
};

/**
 * Send keyboard event of space to given panel.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} target element.
 */
const sendSpaceKeyEvent = function (animationInspector, element) {
  element.focus();
  EventUtils.sendKey("SPACE", element.ownerGlobal);
};

/**
 * Set a node class attribute to the given selector.
 *
 * @param {AnimationInspector} animationInspector
 * @param {String} selector
 * @param {String} cls
 *        e.g. ".ball.still"
 */
const setClassAttribute = async function (animationInspector, selector, cls) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [cls, selector],
    (attributeValue, selectorChild) => {
      const node = content.document.querySelector(selectorChild);
      if (!node) {
        return;
      }

      node.setAttribute("class", attributeValue);
    }
  );
};

/**
 * Set a new style properties to the node for the given selector.
 *
 * @param {AnimationInspector} animationInspector
 * @param {String} selector
 * @param {Object} properties
 *        e.g. {
 *               animationDuration: "1000ms",
 *               animationTimingFunction: "linear",
 *             }
 */
const setEffectTimingAndPlayback = async function (
  animationInspector,
  selector,
  effectTiming,
  playbackRate
) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, playbackRate, effectTiming],
    (selectorChild, playbackRateChild, effectTimingChild) => {
      let selectedAnimation = null;

      for (const animation of content.document.getAnimations()) {
        if (animation.effect.target.matches(selectorChild)) {
          selectedAnimation = animation;
          break;
        }
      }

      if (!selectedAnimation) {
        return;
      }

      selectedAnimation.playbackRate = playbackRateChild;
      selectedAnimation.effect.updateTiming(effectTimingChild);
    }
  );
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
 * Set a new style property declaration to the node for the given selector.
 *
 * @param {AnimationInspector} animationInspector
 * @param {String} selector
 * @param {String} propertyName
 *        e.g. "animationDuration"
 * @param {String} propertyValue
 *        e.g. "5.5s"
 */
const setStyle = async function (
  animationInspector,
  selector,
  propertyName,
  propertyValue
) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [selector, propertyName, propertyValue],
    (selectorChild, propertyNameChild, propertyValueChild) => {
      const node = content.document.querySelector(selectorChild);
      if (!node) {
        return;
      }

      node.style[propertyNameChild] = propertyValueChild;
    }
  );
};

/**
 * Set a new style properties to the node for the given selector.
 *
 * @param {AnimationInspector} animationInspector
 * @param {String} selector
 * @param {Object} properties
 *        e.g. {
 *               animationDuration: "1000ms",
 *               animationTimingFunction: "linear",
 *             }
 */
const setStyles = async function (animationInspector, selector, properties) {
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [properties, selector],
    (propertiesChild, selectorChild) => {
      const node = content.document.querySelector(selectorChild);
      if (!node) {
        return;
      }

      for (const propertyName in propertiesChild) {
        const propertyValue = propertiesChild[propertyName];
        node.style[propertyName] = propertyValue;
      }
    }
  );
};

/**
 * Wait until current time of animations will be changed to given current time.
 *
 * @param {AnimationInspector} animationInspector
 * @param {Number} currentTime
 */
const waitUntilCurrentTimeChangedAt = async function (
  animationInspector,
  currentTime
) {
  info(`Wait until current time will be change to ${currentTime}`);
  await waitUntil(() =>
    animationInspector.state.animations.every(a => {
      return a.state.currentTime === currentTime;
    })
  );
};

/**
 * Wait until animations' play state will be changed to given state.
 *
 * @param {Array} animationInspector
 * @param {String} state
 */
const waitUntilAnimationsPlayState = async function (
  animationInspector,
  state
) {
  info(`Wait until play state will be change to ${state}`);
  await waitUntil(() =>
    animationInspector.state.animations.every(a => a.state.playState === state)
  );
};

/**
 * Return count of graph that animation inspector is displaying.
 *
 * @param {AnimationInspector} animationInspector
 * @param {DOMElement} panel
 * @return {Number} count
 */
const getDisplayedGraphCount = (animationInspector, panel) => {
  const animationLength = animationInspector.state.animations.length;
  if (animationLength === 0) {
    return 0;
  }

  const inspectionPanelEl = panel.querySelector(".progress-inspection-panel");
  const itemEl = panel.querySelector(".animation-item");
  const listEl = panel.querySelector(".animation-list");
  const itemHeight = itemEl.offsetHeight;
  // This calculation should be same as AnimationListContainer.updateDisplayableRange.
  const count = Math.floor(listEl.offsetHeight / itemHeight) + 1;
  const index = Math.floor(inspectionPanelEl.scrollTop / itemHeight);

  return animationLength > index + count ? count : animationLength - index;
};

/**
 * Check whether the animations are pausing.
 *
 * @param {AnimationInspector} animationInspector
 */
function assertAnimationsPausing(animationInspector) {
  assertAnimationsPausingOrRunning(animationInspector, true);
}

/**
 * Check whether the animations are pausing/running.
 *
 * @param {AnimationInspector} animationInspector
 * @param {boolean} shouldPause
 */
function assertAnimationsPausingOrRunning(animationInspector, shouldPause) {
  const hasRunningAnimation = animationInspector.state.animations.some(
    ({ state }) => state.playState === "running"
  );

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
 */
function assertAnimationsRunning(animationInspector) {
  assertAnimationsPausingOrRunning(animationInspector, false);
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
  ok(stopEl, `stop element at offset ${offset} should exist`);
  is(
    stopEl.getAttribute("stop-color"),
    expectedColor,
    `stop-color of stop element at offset ${offset} should be ${expectedColor}`
  );
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
  ok(
    isExpectedPath(pathEl, hasClosePath, expectedValues),
    "All of path segments are correct"
  );
}

function isExpectedPath(pathEl, hasClosePath, expectedValues) {
  const pathSegList = pathEl.pathSegList;
  if (!pathSegList) {
    return false;
  }

  if (
    !expectedValues.every(value =>
      isPassingThrough(pathSegList, value.x, value.y)
    )
  ) {
    return false;
  }

  if (hasClosePath) {
    const closePathSeg = pathSegList.getItem(pathSegList.numberOfItems - 1);
    if (closePathSeg.pathSegType !== closePathSeg.PATHSEG_CLOSEPATH) {
      return false;
    }
  }

  return true;
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
    if (
      previousX <= x &&
      x <= currentX &&
      Math.min(previousY, currentY) <= y &&
      y <= Math.max(previousY, currentY)
    ) {
      return true;
    }
    previousPathSeg = pathSeg;
  }
  return false;
}

/**
 * Return animation item element by the index.
 *
 * @param {DOMElement} panel
 *        #animation-container element.
 * @param {Number} index
 * @return {DOMElement}
 *        Animation item element.
 */
async function findAnimationItemByIndex(panel, index) {
  const itemEls = [...panel.querySelectorAll(".animation-item")];
  const itemEl = itemEls[index];
  itemEl.scrollIntoView(false);

  await waitUntil(
    () =>
      itemEl.querySelector(".animation-target .attrName") &&
      itemEl.querySelector(".animation-computed-timing-path")
  );

  return itemEl;
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
async function findAnimationItemByTargetSelector(panel, selector) {
  for (const itemEl of panel.querySelectorAll(".animation-item")) {
    itemEl.scrollIntoView(false);

    await waitUntil(
      () =>
        itemEl.querySelector(".animation-target .attrName") &&
        itemEl.querySelector(".animation-computed-timing-path")
    );

    const attrNameEl = itemEl.querySelector(".animation-target .attrName");
    const regexp = new RegExp(`\\${selector}(\\.|$)`, "gi");
    if (regexp.exec(attrNameEl.textContent)) {
      return itemEl;
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
  await removeAnimatedElementsExcept(testData.map(t => `.${t.targetClass}`));
  const { animationInspector, panel } = await openAnimationInspector();

  for (const { properties, targetClass } of testData) {
    info(`Checking keyframes graph for ${targetClass}`);
    const onDetailRendered = animationInspector.once(
      "animation-keyframes-rendered"
    );
    await clickOnAnimationByTargetSelector(
      animationInspector,
      panel,
      `.${targetClass}`
    );
    await onDetailRendered;

    for (const property of properties) {
      const {
        name,
        computedValuePathClass,
        expectedPathSegments,
        expectedStopColors,
      } = property;

      const testTarget = `${name} in ${targetClass}`;
      info(`Checking keyframes graph for ${testTarget}`);
      info(`Checking keyframes graph path existence for ${testTarget}`);
      const keyframesGraphPathEl = panel.querySelector(`.${name}`);
      ok(
        keyframesGraphPathEl,
        `The keyframes graph path element of ${testTarget} should be existence`
      );

      info(`Checking computed value path existence for ${testTarget}`);
      const computedValuePathEl = keyframesGraphPathEl.querySelector(
        `.${computedValuePathClass}`
      );
      ok(
        computedValuePathEl,
        `The computed value path element of ${testTarget} should be existence`
      );

      info(`Checking path segments for ${testTarget}`);
      const pathEl = computedValuePathEl.querySelector("path");
      ok(pathEl, `The <path> element of ${testTarget} should be existence`);
      assertPathSegments(pathEl, true, expectedPathSegments);

      if (!expectedStopColors) {
        continue;
      }

      info(`Checking linearGradient for ${testTarget}`);
      const linearGradientEl =
        computedValuePathEl.querySelector("linearGradient");
      ok(
        linearGradientEl,
        `The <linearGradientEl> element of ${testTarget} should be existence`
      );

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
  const adjustedCurrentTimeDiff =
    animation2.currentTime / animation2.playbackRate -
    animation1.currentTime / animation1.playbackRate;
  const createdTimeDiff = animation1.createdTime - animation2.createdTime;
  ok(
    Math.abs(adjustedCurrentTimeDiff - createdTimeDiff) < 0.1,
    "Adjusted time is correct"
  );
}
