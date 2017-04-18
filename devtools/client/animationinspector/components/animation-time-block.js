/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {createNode, createSVGNode, TimeScale, getFormattedAnimationTitle} =
  require("devtools/client/animationinspector/utils");
const {createPathSegments, appendPathElement, DEFAULT_MIN_PROGRESS_THRESHOLD} =
  require("devtools/client/animationinspector/graph-helper");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
      new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// Show max 10 iterations for infinite animations
// to give users a clue that the animation does repeat.
const MAX_INFINITE_ANIMATIONS_ITERATIONS = 10;

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
    const {x, delayX, delayW, endDelayX, endDelayW} =
      TimeScale.getAnimationDimensions(animation);

    // Animation summary graph element.
    const summaryEl = createSVGNode({
      parent: this.containerEl,
      nodeType: "svg",
      attributes: {
        "class": "summary",
        "preserveAspectRatio": "none",
        "style": `left: ${ x - (state.delay > 0 ? delayW : 0) }%`
      }
    });

    // Total displayed duration
    const totalDisplayedDuration = state.playbackRate * TimeScale.getDuration();

    // Calculate stroke height in viewBox to display stroke of path.
    const strokeHeightForViewBox = 0.5 / this.containerEl.clientHeight;

    // Set viewBox
    summaryEl.setAttribute("viewBox",
                           `${ state.delay < 0 ? state.delay : 0 }
                            -${ 1 + strokeHeightForViewBox }
                            ${ totalDisplayedDuration }
                            ${ 1 + strokeHeightForViewBox * 2 }`);

    // Get a helper function that returns the path segment of timing-function.
    const segmentHelper = getSegmentHelper(state, this.win);

    // Minimum segment duration is the duration of one pixel.
    const minSegmentDuration =
      totalDisplayedDuration / this.containerEl.clientWidth;
    // Minimum progress threshold.
    let minProgressThreshold = DEFAULT_MIN_PROGRESS_THRESHOLD;
    // If the easing is step function,
    // minProgressThreshold should be changed by the steps.
    const stepFunction = state.easing.match(/steps\((\d+)/);
    if (stepFunction) {
      minProgressThreshold = 1 / (parseInt(stepFunction[1], 10) + 1);
    }

    // Starting time of main iteration.
    let mainIterationStartTime = 0;
    let iterationStart = state.iterationStart;
    let iterationCount = state.iterationCount ? state.iterationCount : Infinity;

    // Append delay.
    if (state.delay > 0) {
      renderDelay(summaryEl, state, segmentHelper);
      mainIterationStartTime = state.delay;
    } else {
      const negativeDelayCount = -state.delay / state.duration;
      // Move to forward the starting point for negative delay.
      iterationStart += negativeDelayCount;
      // Consume iteration count by negative delay.
      if (iterationCount !== Infinity) {
        iterationCount -= negativeDelayCount;
      }
    }

    // Append 1st section of iterations,
    // This section is only useful in cases where iterationStart has decimals.
    // e.g.
    // if { iterationStart: 0.25, iterations: 3 }, firstSectionCount is 0.75.
    const firstSectionCount =
      iterationStart % 1 === 0
      ? 0 : Math.min(iterationCount, 1) - iterationStart % 1;
    if (firstSectionCount) {
      renderFirstIteration(summaryEl, state, mainIterationStartTime,
                           firstSectionCount, minSegmentDuration,
                           minProgressThreshold, segmentHelper);
    }

    if (iterationCount === Infinity) {
      // If the animation repeats infinitely,
      // we fill the remaining area with iteration paths.
      renderInfinity(summaryEl, state, mainIterationStartTime,
                     firstSectionCount, totalDisplayedDuration,
                     minSegmentDuration, minProgressThreshold, segmentHelper);
    } else {
      // Otherwise, we show remaining iterations, endDelay and fill.

      // Append forwards fill-mode.
      if (state.fill === "both" || state.fill === "forwards") {
        renderForwardsFill(summaryEl, state, mainIterationStartTime,
                           iterationCount, totalDisplayedDuration,
                           segmentHelper);
      }

      // Append middle section of iterations.
      // e.g.
      // if { iterationStart: 0.25, iterations: 3 }, middleSectionCount is 2.
      const middleSectionCount =
        Math.floor(iterationCount - firstSectionCount);
      renderMiddleIterations(summaryEl, state, mainIterationStartTime,
                             firstSectionCount, middleSectionCount,
                             minSegmentDuration, minProgressThreshold,
                             segmentHelper);

      // Append last section of iterations, if there is remaining iteration.
      // e.g.
      // if { iterationStart: 0.25, iterations: 3 }, lastSectionCount is 0.25.
      const lastSectionCount =
        iterationCount - middleSectionCount - firstSectionCount;
      if (lastSectionCount) {
        renderLastIteration(summaryEl, state, mainIterationStartTime,
                            firstSectionCount, middleSectionCount,
                            lastSectionCount, minSegmentDuration,
                            minProgressThreshold, segmentHelper);
      }

      // Append endDelay.
      if (state.endDelay > 0) {
        renderEndDelay(summaryEl, state,
                       mainIterationStartTime, iterationCount, segmentHelper);
      }
    }

    // Append negative delay (which overlap the animation).
    if (state.delay < 0) {
      segmentHelper.animation.effect.timing.fill = "both";
      segmentHelper.asOriginalBehavior = false;
      renderNegativeDelayHiddenProgress(summaryEl, state, minSegmentDuration,
                                        minProgressThreshold, segmentHelper);
    }
    // Append negative endDelay (which overlap the animation).
    if (state.iterationCount && state.endDelay < 0) {
      if (segmentHelper.asOriginalBehavior) {
        segmentHelper.animation.effect.timing.fill = "both";
        segmentHelper.asOriginalBehavior = false;
      }
      renderNegativeEndDelayHiddenProgress(summaryEl, state,
                                           minSegmentDuration,
                                           minProgressThreshold,
                                           segmentHelper);
    }

    // The animation name is displayed over the animation.
    createNode({
      parent: createNode({
        parent: this.containerEl,
        attributes: {
          "class": "name",
          "title": this.getTooltipText(state)
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
          "class": "delay"
                   + (state.delay < 0 ? " negative" : " positive")
                   + (state.fill === "both" ||
                      state.fill === "backwards" ? " fill" : ""),
          "style": `left:${ delayX }%; width:${ delayW }%;`
        }
      });
    }

    // endDelay
    if (state.iterationCount && state.endDelay) {
      createNode({
        parent: this.containerEl,
        attributes: {
          "class": "end-delay"
                   + (state.endDelay < 0 ? " negative" : " positive")
                   + (state.fill === "both" ||
                      state.fill === "forwards" ? " fill" : ""),
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

    // Adding the easing if it is not "linear".
    if (state.easing && state.easing !== "linear") {
      text += L10N.getStr("player.animationOverallEasingLabel") + " ";
      text += state.easing;
      text += "\n";
    }

    // Adding the fill mode.
    if (state.fill) {
      text += L10N.getStr("player.animationFillLabel") + " ";
      text += state.fill;
      text += "\n";
    }

    // Adding the direction mode if it is not "normal".
    if (state.direction && state.direction !== "normal") {
      text += L10N.getStr("player.animationDirectionLabel") + " ";
      text += state.direction;
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
 * Render delay section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderDelay(parentEl, state, segmentHelper) {
  const startSegment = segmentHelper.getSegment(0);
  const endSegment = { x: state.delay, y: startSegment.y };
  appendPathElement(parentEl, [startSegment, endSegment], "delay-path");
}

/**
 * Render first iteration section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderFirstIteration(parentEl, state, mainIterationStartTime,
                              firstSectionCount, minSegmentDuration,
                              minProgressThreshold, segmentHelper) {
  const startTime = mainIterationStartTime;
  const endTime = startTime + firstSectionCount * state.duration;
  const segments =
    createPathSegments(startTime, endTime, minSegmentDuration,
                       minProgressThreshold, segmentHelper);
  appendPathElement(parentEl, segments, "iteration-path");
}

/**
 * Render middle iterations section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} middleSectionCount - Iteration count of middle section.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderMiddleIterations(parentEl, state, mainIterationStartTime,
                                firstSectionCount, middleSectionCount,
                                minSegmentDuration, minProgressThreshold,
                                segmentHelper) {
  const offset = mainIterationStartTime + firstSectionCount * state.duration;
  for (let i = 0; i < middleSectionCount; i++) {
    // Get the path segments of each iteration.
    const startTime = offset + i * state.duration;
    const endTime = startTime + state.duration;
    const segments =
      createPathSegments(startTime, endTime, minSegmentDuration,
                         minProgressThreshold, segmentHelper);
    appendPathElement(parentEl, segments, "iteration-path");
  }
}

/**
 * Render last iteration section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} middleSectionCount - Iteration count of middle section.
 * @param {Number} lastSectionCount - Iteration count of last section.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderLastIteration(parentEl, state, mainIterationStartTime,
                             firstSectionCount, middleSectionCount,
                             lastSectionCount, minSegmentDuration,
                             minProgressThreshold, segmentHelper) {
  const startTime = mainIterationStartTime +
                      (firstSectionCount + middleSectionCount) * state.duration;
  const endTime = startTime + lastSectionCount * state.duration;
  const segments =
    createPathSegments(startTime, endTime, minSegmentDuration,
                       minProgressThreshold, segmentHelper);
  appendPathElement(parentEl, segments, "iteration-path");
}

/**
 * Render Infinity iterations.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} totalDuration - Displayed max duration.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderInfinity(parentEl, state, mainIterationStartTime,
                        firstSectionCount, totalDuration, minSegmentDuration,
                        minProgressThreshold, segmentHelper) {
  // Calculate the number of iterations to display,
  // with a maximum of MAX_INFINITE_ANIMATIONS_ITERATIONS
  let uncappedInfinityIterationCount =
    (totalDuration - firstSectionCount * state.duration) / state.duration;
  // If there is a small floating point error resulting in, e.g. 1.0000001
  // ceil will give us 2 so round first.
  uncappedInfinityIterationCount =
    parseFloat(uncappedInfinityIterationCount.toPrecision(6));
  const infinityIterationCount =
    Math.min(MAX_INFINITE_ANIMATIONS_ITERATIONS,
             Math.ceil(uncappedInfinityIterationCount));

  // Append first full iteration path.
  const firstStartTime =
    mainIterationStartTime + firstSectionCount * state.duration;
  const firstEndTime = firstStartTime + state.duration;
  const firstSegments =
    createPathSegments(firstStartTime, firstEndTime, minSegmentDuration,
                       minProgressThreshold, segmentHelper);
  appendPathElement(parentEl, firstSegments, "iteration-path infinity");

  // Append other iterations. We can copy first segments.
  const isAlternate = state.direction.match(/alternate/);
  for (let i = 1; i < infinityIterationCount; i++) {
    const startTime = firstStartTime + i * state.duration;
    let segments;
    if (isAlternate && i % 2) {
      // Copy as reverse.
      segments = firstSegments.map(segment => {
        return { x: firstEndTime - segment.x + startTime, y: segment.y };
      });
    } else {
      // Copy as is.
      segments = firstSegments.map(segment => {
        return { x: segment.x - firstStartTime + startTime, y: segment.y };
      });
    }
    appendPathElement(parentEl, segments, "iteration-path infinity copied");
  }
}

/**
 * Render endDelay section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} iterationCount - Whole iteration count.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderEndDelay(parentEl, state,
                        mainIterationStartTime, iterationCount, segmentHelper) {
  const startTime = mainIterationStartTime + iterationCount * state.duration;
  const startSegment = segmentHelper.getSegment(startTime);
  const endSegment = { x: startTime + state.endDelay, y: startSegment.y };
  appendPathElement(parentEl, [startSegment, endSegment], "enddelay-path");
}

/**
 * Render forwards fill section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} iterationCount - Whole iteration count.
 * @param {Number} totalDuration - Displayed max duration.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderForwardsFill(parentEl, state, mainIterationStartTime,
                            iterationCount, totalDuration, segmentHelper) {
  const startTime = mainIterationStartTime + iterationCount * state.duration +
                      (state.endDelay > 0 ? state.endDelay : 0);
  const startSegment = segmentHelper.getSegment(startTime);
  const endSegment = { x: totalDuration, y: startSegment.y };
  appendPathElement(parentEl, [startSegment, endSegment], "fill-forwards-path");
}

/**
 * Render hidden progress of negative delay.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderNegativeDelayHiddenProgress(parentEl, state, minSegmentDuration,
                                           minProgressThreshold,
                                           segmentHelper) {
  const startTime = state.delay;
  const endTime = 0;
  const segments =
    createPathSegments(startTime, endTime, minSegmentDuration,
                       minProgressThreshold, segmentHelper);
  appendPathElement(parentEl, segments, "delay-path negative");
}

/**
 * Render hidden progress of negative endDelay.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper - The object returned by getSegmentHelper.
 */
function renderNegativeEndDelayHiddenProgress(parentEl, state,
                                              minSegmentDuration,
                                              minProgressThreshold,
                                              segmentHelper) {
  const endTime = state.delay + state.iterationCount * state.duration;
  const startTime = endTime + state.endDelay;
  const segments =
    createPathSegments(startTime, endTime, minSegmentDuration,
                       minProgressThreshold, segmentHelper);
  appendPathElement(parentEl, segments, "enddelay-path negative");
}

/**
 * Get a helper function which returns the segment coord from given time.
 * @param {Object} state - animation state
 * @param {Object} win - window object
 * @return {Object} A segmentHelper object that has the following properties:
 * - animation: The script animation used to get the progress
 * - endTime: The end time of the animation
 * - asOriginalBehavior: The spec is that the progress of animation is changed
 *                       if the time of setCurrentTime is during the endDelay.
 *                       Likewise, in case the time is less than 0.
 *                       If this flag is true, we prevent the time
 *                       to make the same animation behavior as the original.
 * - getSegment: Helper function that, given a time,
 *               will calculate the progress through the dummy animation.
 */
function getSegmentHelper(state, win) {
  // Create a dummy Animation timing data as the
  // state object we're being passed in.
  const timing = Object.assign({}, state, {
    iterations: state.iterationCount ? state.iterationCount : Infinity
  });

  // Create a dummy Animation with the given timing.
  const dummyAnimation =
    new win.Animation(new win.KeyframeEffect(null, null, timing), null);

  // Returns segment helper object.
  return {
    animation: dummyAnimation,
    endTime: dummyAnimation.effect.getComputedTiming().endTime,
    asOriginalBehavior: true,
    getSegment: function (time) {
      if (this.asOriginalBehavior) {
        // If the given time is less than 0, returned progress is 0.
        if (time < 0) {
          return { x: time, y: 0 };
        }
        // Avoid to apply over endTime.
        this.animation.currentTime = time < this.endTime ? time : this.endTime;
      } else {
        this.animation.currentTime = time;
      }
      const progress = this.animation.effect.getComputedTiming().progress;
      return { x: time, y: Math.max(progress, 0) };
    }
  };
}
