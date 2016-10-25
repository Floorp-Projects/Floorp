/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {createNode, TimeScale} = require("devtools/client/animationinspector/utils");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/locale/animationinspector.properties");

// In the createPathSegments function, an animation duration is divided by
// DURATION_RESOLUTION in order to draw the way the animation progresses.
// But depending on the timing-function, we may be not able to make the graph
// smoothly progress if this resolution is not high enough.
// So, if the difference of animation progress between 2 divisions is more than
// MIN_PROGRESS_THRESHOLD, then createPathSegments re-divides
// by DURATION_RESOLUTION.
// DURATION_RESOLUTION shoud be integer and more than 2.
const DURATION_RESOLUTION = 4;
// MIN_PROGRESS_THRESHOLD shoud be between more than 0 to 1.
const MIN_PROGRESS_THRESHOLD = 0.1;
// SVG namespace
const SVG_NS = "http://www.w3.org/2000/svg";
// ID of mask for fade effect.
const FADE_MASK_ID = "animationinspector-fade-mask";
// ID of gradient element.
const FADE_GRADIENT_ID = "animationinspector-fade-gradient";

/**
 * UI component responsible for displaying a single animation timeline, which
 * basically looks like a rectangle that shows the delay and iterations.
 */
function AnimationTimeBlock() {
  EventEmitter.decorate(this);
  this.onClick = this.onClick.bind(this);
}

exports.AnimationTimeBlock = AnimationTimeBlock;

AnimationTimeBlock.prototype = {
  init: function (containerEl) {
    this.containerEl = containerEl;
    this.containerEl.addEventListener("click", this.onClick);
  },

  destroy: function () {
    this.containerEl.removeEventListener("click", this.onClick);
    this.unrender();
    this.containerEl = null;
    this.animation = null;
  },

  unrender: function () {
    while (this.containerEl.firstChild) {
      this.containerEl.firstChild.remove();
    }
  },

  render: function (animation) {
    this.unrender();

    this.animation = animation;
    let {state} = this.animation;

    // Create a container element to hold the delay and iterations.
    // It is positioned according to its delay (divided by the playbackrate),
    // and its width is according to its duration (divided by the playbackrate).
    let {x, iterationW, delayX, delayW, negativeDelayW, endDelayX, endDelayW} =
      TimeScale.getAnimationDimensions(animation);

    // background properties for .iterations element
    let backgroundIterations = TimeScale.getIterationsBackgroundData(animation);

    // Displayed total duration
    const totalDuration = TimeScale.getDuration() * state.playbackRate;

    // Get a helper function that returns the path segment of timing-function.
    const segmentHelperFn = getSegmentHelper(state, this.win);

    // Minimum segment duration is the duration of one pixel.
    const minSegmentDuration = totalDuration / this.containerEl.clientWidth;
    // Minimum progress threshold.
    let minProgressThreshold = MIN_PROGRESS_THRESHOLD;
    // If the easing is step function,
    // minProgressThreshold should be changed by the steps.
    const stepFunction = state.easing.match(/steps\((\d+)/);
    if (stepFunction) {
      minProgressThreshold = 1 / (parseInt(stepFunction[1], 10) + 1);
    }

    // Calculate stroke height in viewBox to display stroke of path.
    const strokeHeightForViewBox = 0.5 / this.containerEl.clientHeight;

    // Animation summary graph element.
    const summaryEl = createNode({
      parent: this.containerEl,
      namespace: SVG_NS,
      nodeType: "svg",
      attributes: {
        "class": "summary",
        "viewBox": `${ state.delay < 0 ? state.delay : 0 }
                    -${ 1 + strokeHeightForViewBox }
                    ${ totalDuration }
                    ${ 1 + strokeHeightForViewBox * 2 }`,
        "preserveAspectRatio": "none",
        "style": `left: ${ x - (state.delay > 0 ? delayW : 0) }%;`,
      }
    });

    // Iteration count
    const iterationCount = state.iterationCount ? state.iterationCount : 1;

    // Append forwards fill-mode.
    if (state.fill === "both" || state.fill === "forwards") {
      renderForwardsFill(summaryEl, state, iterationCount,
                         totalDuration, segmentHelperFn);
    }

    // Append delay.
    if (state.delay > 0) {
      renderDelay(summaryEl, state, segmentHelperFn);
    }

    // Append 1st section of iterations,
    // if this animation has decimal iterationStart.
    const firstSectionCount =
      state.iterationStart % 1 === 0
      ? 0 : Math.min(iterationCount, 1) - state.iterationStart % 1;
    if (firstSectionCount) {
      renderFirstIteration(summaryEl, state, firstSectionCount,
                           minSegmentDuration, minProgressThreshold,
                           segmentHelperFn);
    }

    // Append middle section of iterations.
    const middleSectionCount =
      Math.floor(state.iterationCount - firstSectionCount);
    renderMiddleIterations(summaryEl, state, firstSectionCount,
                           middleSectionCount, minSegmentDuration,
                           minProgressThreshold, segmentHelperFn);

    // Append last section of iterations, if there is remaining iteration.
    const lastSectionCount =
      iterationCount - middleSectionCount - firstSectionCount;
    if (lastSectionCount) {
      renderLastIteration(summaryEl, state, firstSectionCount,
                          middleSectionCount, lastSectionCount,
                          minSegmentDuration, minProgressThreshold,
                          segmentHelperFn);
    }

    // Append endDelay.
    if (state.endDelay > 0) {
      renderEndDelay(summaryEl, state, iterationCount, segmentHelperFn);
    }

    // The animation name is displayed over the iterations.
    // Note that in case of negative delay, it is pushed towards the right so
    // the delay element does not overlap.
    createNode({
      parent: createNode({
        parent: this.containerEl,
        attributes: {
          "class": "name",
          "title": this.getTooltipText(state),
          // Place the name at the same position as the iterations, but make
          // space for the negative delay if any.
          "style": `left:${x + negativeDelayW}%;
                    width:${iterationW - negativeDelayW}%;`
        },
      }),
      textContent: state.name
    });

    // Delay.
    if (state.delay) {
      // Negative delays need to start at 0.
      createNode({
        parent: this.containerEl,
        attributes: {
          "class": "delay" + (state.delay < 0 ? " negative" : ""),
          "style": `left:${ delayX }%; width:${ delayW }%;`
        }
      });
    }

    // endDelay
    if (state.endDelay) {
      createNode({
        parent: this.containerEl,
        attributes: {
          "class": "end-delay" + (state.endDelay < 0 ? " negative" : ""),
          "style": `left:${ endDelayX }%; width:${ endDelayW }%;`
        }
      });
    }
  },

  getTooltipText: function (state) {
    let getTime = time => L10N.getFormatStr("player.timeLabel",
                                            L10N.numberWithDecimals(time / 1000, 2));

    let text = "";

    // Adding the name.
    text += getFormattedAnimationTitle({state});
    text += "\n";

    // Adding the delay.
    if (state.delay) {
      text += L10N.getStr("player.animationDelayLabel") + " ";
      text += getTime(state.delay);
      text += "\n";
    }

    // Adding the duration.
    text += L10N.getStr("player.animationDurationLabel") + " ";
    text += getTime(state.duration);
    text += "\n";

    // Adding the endDelay.
    if (state.endDelay) {
      text += L10N.getStr("player.animationEndDelayLabel") + " ";
      text += getTime(state.endDelay);
      text += "\n";
    }

    // Adding the iteration count (the infinite symbol, or an integer).
    if (state.iterationCount !== 1) {
      text += L10N.getStr("player.animationIterationCountLabel") + " ";
      text += state.iterationCount ||
              L10N.getStr("player.infiniteIterationCountText");
      text += "\n";
    }

    // Adding the iteration start.
    if (state.iterationStart !== 0) {
      let iterationStartTime = state.iterationStart * state.duration / 1000;
      text += L10N.getFormatStr("player.animationIterationStartLabel",
                                state.iterationStart,
                                L10N.numberWithDecimals(iterationStartTime, 2));
      text += "\n";
    }

    // Adding the playback rate if it's different than 1.
    if (state.playbackRate !== 1) {
      text += L10N.getStr("player.animationRateLabel") + " ";
      text += state.playbackRate;
      text += "\n";
    }

    // Adding a note that the animation is running on the compositor thread if
    // needed.
    if (state.propertyState) {
      if (state.propertyState
               .every(propState => propState.runningOnCompositor)) {
        text += L10N.getStr("player.allPropertiesOnCompositorTooltip");
      } else if (state.propertyState
                      .some(propState => propState.runningOnCompositor)) {
        text += L10N.getStr("player.somePropertiesOnCompositorTooltip");
      }
    } else if (state.isRunningOnCompositor) {
      text += L10N.getStr("player.runningOnCompositorTooltip");
    }

    return text;
  },

  onClick: function (e) {
    e.stopPropagation();
    this.emit("selected", this.animation);
  },

  get win() {
    return this.containerEl.ownerDocument.defaultView;
  }
};

/**
 * Get a formatted title for this animation. This will be either:
 * "some-name", "some-name : CSS Transition", "some-name : CSS Animation",
 * "some-name : Script Animation", or "Script Animation", depending
 * if the server provides the type, what type it is and if the animation
 * has a name
 * @param {AnimationPlayerFront} animation
 */
function getFormattedAnimationTitle({state}) {
  // Older servers don't send a type, and only know about
  // CSSAnimations and CSSTransitions, so it's safe to use
  // just the name.
  if (!state.type) {
    return state.name;
  }

  // Script-generated animations may not have a name.
  if (state.type === "scriptanimation" && !state.name) {
    return L10N.getStr("timeline.scriptanimation.unnamedLabel");
  }

  return L10N.getFormatStr(`timeline.${state.type}.nameLabel`, state.name);
}

/**
 * Render delay section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {function} getSegment - The function of getSegmentHelper.
 */
function renderDelay(parentEl, state, getSegment) {
  const startSegment = getSegment(0);
  const endSegment = { x: state.delay, y: startSegment.y };
  appendPathElement(parentEl, [startSegment, endSegment], "delay-path");
}

/**
 * Render first iteration section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {function} getSegment - The function of getSegmentHelper.
 */
function renderFirstIteration(parentEl, state, firstSectionCount,
                              minSegmentDuration, minProgressThreshold,
                              getSegment) {
  const startTime = state.delay;
  const endTime = startTime + firstSectionCount * state.duration;
  const segments =
    createPathSegments(startTime, endTime, minSegmentDuration,
                       minProgressThreshold, getSegment);
  appendPathElement(parentEl, segments, "iteration-path");
}

/**
 * Render middle iterations section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} middleSectionCount - Iteration count of middle section.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {function} getSegment - The function of getSegmentHelper.
 */
function renderMiddleIterations(parentEl, state, firstSectionCount,
                                middleSectionCount, minSegmentDuration,
                                minProgressThreshold, getSegment) {
  const offset = state.delay + firstSectionCount * state.duration;
  for (let i = 0; i < middleSectionCount; i++) {
    // Get the path segments of each iteration.
    const startTime = offset + i * state.duration;
    const endTime = startTime + state.duration;
    const segments =
      createPathSegments(startTime, endTime, minSegmentDuration,
                         minProgressThreshold, getSegment);
    appendPathElement(parentEl, segments, "iteration-path");
  }
}

/**
 * Render last iteration section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} middleSectionCount - Iteration count of middle section.
 * @param {Number} lastSectionCount - Iteration count of last section.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {function} getSegment - The function of getSegmentHelper.
 */
function renderLastIteration(parentEl, state, firstSectionCount,
                             middleSectionCount, lastSectionCount,
                             minSegmentDuration, minProgressThreshold,
                             getSegment) {
  const startTime = state.delay + firstSectionCount * state.duration
                    + middleSectionCount * state.duration;
  const endTime = startTime + lastSectionCount * state.duration;
  const segments =
    createPathSegments(startTime, endTime, minSegmentDuration,
                       minProgressThreshold, getSegment);
  appendPathElement(parentEl, segments, "iteration-path");
}

/**
 * Render endDelay section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} iterationCount - Whole iteration count.
 * @param {function} getSegment - The function of getSegmentHelper.
 */
function renderEndDelay(parentEl, state, iterationCount, getSegment) {
  const startTime = state.delay + iterationCount * state.duration;
  const startSegment = getSegment(startTime);
  const endSegment = { x: startTime + state.endDelay, y: startSegment.y };
  appendPathElement(parentEl, [startSegment, endSegment], "enddelay-path");
}

/**
 * Render forwards fill section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} iterationCount - Whole iteration count.
 * @param {Number} totalDuration - Displayed max duration.
 * @param {function} getSegment - The function of getSegmentHelper.
 */
function renderForwardsFill(parentEl, state, iterationCount,
                            totalDuration, getSegment) {
  const startTime = state.delay + iterationCount * state.duration +
                    (state.endDelay > 0 ? state.endDelay : 0);
  const startSegment = getSegment(startTime);
  const endSegment = { x: totalDuration, y: startSegment.y };
  const pathEl = appendPathElement(parentEl, [startSegment, endSegment],
                                   "fill-forwards-path");
  appendFadeEffect(parentEl, pathEl);
}

/**
 * Get a helper function which returns the segment coord from given time.
 * @param {Object} state - animation state
 * @param {Object} win - window object
 * @return {function} getSegmentHelper
 */
function getSegmentHelper(state, win) {
  // Create a dummy Animation timing data as the
  // state object we're being passed in.
  const timing = Object.assign({}, state, {
    iterations: state.iterationCount ? state.iterationCount : 1
  });
  // Create a dummy Animation with the given timing.
  const dummyAnimation =
    new win.Animation(new win.KeyframeEffect(null, null, timing), null);
  const endTime = dummyAnimation.effect.getComputedTiming().endTime;
  // Return a helper function that, given a time,
  // will calculate the progress through the dummy animation.
  return time => {
    // If the given time is less than 0, returned progress is 0.
    if (time < 0) {
      return { x: time, y: 0 };
    }
    dummyAnimation.currentTime =
      time < endTime ? time : endTime;
    const progress = dummyAnimation.effect.getComputedTiming().progress;
    return { x: time, y: Math.max(progress, 0) };
  };
}

/**
 * Create the path segments from given parameters.
 * @param {Number} startTime - Starting time of animation.
 * @param {Number} endTime - Ending time of animation.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {function} getSegment - The function of getSegmentHelper.
 * @return {Array} path segments -
 *                 [{x: {Number} time, y: {Number} progress}, ...]
 */
function createPathSegments(startTime, endTime, minSegmentDuration,
                            minProgressThreshold, getSegment) {
  // If the duration is too short, early return.
  if (endTime - startTime < minSegmentDuration) {
    return [getSegment(startTime), getSegment(endTime)];
  }

  // Otherwise, start creating segments.
  let pathSegments = [];

  // Append the segment for the startTime position.
  const startTimeSegment = getSegment(startTime);
  pathSegments.push(startTimeSegment);
  let previousSegment = startTimeSegment;

  // Split the duration in equal intervals, and iterate over them.
  // See the definition of DURATION_RESOLUTION for more information about this.
  const interval = (endTime - startTime) / DURATION_RESOLUTION;
  for (let index = 1; index <= DURATION_RESOLUTION; index++) {
    // Create a segment for this interval.
    const currentSegment = getSegment(startTime + index * interval);

    // If the distance between the Y coordinate (the animation's progress) of
    // the previous segment and the Y coordinate of the current segment is too
    // large, then recurse with a smaller duration to get more details
    // in the graph.
    if (Math.abs(currentSegment.y - previousSegment.y) > minProgressThreshold) {
      // Divide the current interval (excluding start and end bounds
      // by adding/subtracting 1ms).
      pathSegments = pathSegments.concat(
        createPathSegments(previousSegment.x + 1, currentSegment.x - 1,
                           minSegmentDuration, minProgressThreshold,
                           getSegment));
    }

    pathSegments.push(currentSegment);
    previousSegment = currentSegment;
  }

  return pathSegments;
}

/**
 * Append path element.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Array} pathSegments - Path segments. Please see createPathSegments.
 * @param {String} cls - Class name.
 * @return {Element} path element.
 */
function appendPathElement(parentEl, pathSegments, cls) {
  // Create path string.
  let path = `M${ pathSegments[0].x },0`;
  pathSegments.forEach(pathSegment => {
    path += ` L${ pathSegment.x },${ pathSegment.y }`;
  });
  path += ` L${ pathSegments[pathSegments.length - 1].x },0 Z`;
  // Append and return the path element.
  return createNode({
    parent: parentEl,
    namespace: SVG_NS,
    nodeType: "path",
    attributes: {
      "d": path,
      "class": cls,
      "vector-effect": "non-scaling-stroke",
      "transform": "scale(1, -1)"
    }
  });
}

/**
 * Append fade effect.
 * @param {Element} parentEl - Parent element of this appended element.
 * @param {Element} el - Append fade effect to this element.
 */
function appendFadeEffect(parentEl, el) {
  // We can re-use mask element for fade.
  // Keep the defs element in SVG element of given parentEl.
  if (!parentEl.ownerDocument.body.querySelector(`#${ FADE_MASK_ID }`)) {
    const svgEl = parentEl.closest(".summary");
    const defsEl = createNode({
      parent: svgEl,
      namespace: SVG_NS,
      nodeType: "defs"
    });
    const gradientEl = createNode({
      parent: defsEl,
      namespace: SVG_NS,
      nodeType: "linearGradient",
      attributes: {
        "id": FADE_GRADIENT_ID
      }
    });
    createNode({
      parent: gradientEl,
      namespace: SVG_NS,
      nodeType: "stop",
      attributes: {
        "offset": 0
      }
    });
    createNode({
      parent: gradientEl,
      namespace: SVG_NS,
      nodeType: "stop",
      attributes: {
        "offset": 1
      }
    });

    const maskEl = createNode({
      parent: defsEl,
      namespace: SVG_NS,
      nodeType: "mask",
      attributes: {
        "id": FADE_MASK_ID,
        "maskContentUnits": "objectBoundingBox",
      }
    });
    createNode({
      parent: maskEl,
      namespace: SVG_NS,
      nodeType: "rect",
      attributes: {
        "y": `${ 1 + svgEl.viewBox.animVal.y }`,
        "width": "1",
        "height": `${ svgEl.viewBox.animVal.height }`,
        "fill": `url(#${ FADE_GRADIENT_ID })`
      }
    });
  }
  el.setAttribute("mask", `url(#${ FADE_MASK_ID })`);
}
