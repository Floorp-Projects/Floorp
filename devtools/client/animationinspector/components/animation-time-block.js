/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/old-event-emitter");
const {createNode, createSVGNode, TimeScale, getFormattedAnimationTitle} =
  require("devtools/client/animationinspector/utils");
const {SummaryGraphHelper, getPreferredKeyframesProgressThreshold,
       getPreferredProgressThreshold} =
  require("devtools/client/animationinspector/graph-helper");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
      new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// Show max 10 iterations for infinite animations
// to give users a clue that the animation does repeat.
const MAX_INFINITE_ANIMATIONS_ITERATIONS = 10;

// Minimum opacity for semitransparent fill color for keyframes's easing graph.
const MIN_KEYFRAMES_EASING_OPACITY = .5;

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

  render: function (animation, tracks) {
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

    // Minimum segment duration is the duration of one pixel.
    const minSegmentDuration = totalDisplayedDuration / this.containerEl.clientWidth;
    // Minimum progress threshold for effect timing.
    const minEffectProgressThreshold = getPreferredProgressThreshold(state.easing);

    // Render summary graph.
    // The summary graph is constructed from keyframes's easing and effect timing.
    const graphHelper = new SummaryGraphHelper(this.win, state, minSegmentDuration);
    renderKeyframesEasingGraph(summaryEl, state, totalDisplayedDuration,
                               minEffectProgressThreshold, tracks, graphHelper);
    if (state.easing !== "linear") {
      renderEffectEasingGraph(summaryEl, state, totalDisplayedDuration,
                              minEffectProgressThreshold, graphHelper);
    }
    graphHelper.destroy();

    // The animation name is displayed over the animation.
    const nameEl = createNode({
      parent: this.containerEl,
      attributes: {
        "class": "name",
        "title": this.getTooltipText(state)
      }
    });

    createSVGNode({
      parent: createSVGNode({
        parent: nameEl,
        nodeType: "svg",
      }),
      nodeType: "text",
      attributes: {
        "y": "50%",
        "x": "100%",
      },
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
 * Render keyframes's easing graph.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {float} totalDisplayedDuration - Displayable total duration.
 * @param {float} minEffectProgressThreshold - Minimum progress threshold for effect.
 * @param {Object} tracks - The value of AnimationsTimeline.getTracks().
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderKeyframesEasingGraph(parentEl, state, totalDisplayedDuration,
                                    minEffectProgressThreshold, tracks, graphHelper) {
  const keyframesList = getOffsetAndEasingOnlyKeyframesList(tracks);
  const keyframeEasingOpacity = Math.max(1 / keyframesList.length,
                                         MIN_KEYFRAMES_EASING_OPACITY);
  for (let keyframes of keyframesList) {
    const minProgressTreshold =
      Math.min(minEffectProgressThreshold,
               getPreferredKeyframesProgressThreshold(keyframes));
    graphHelper.setMinProgressThreshold(minProgressTreshold);
    graphHelper.setKeyframes(keyframes);
    graphHelper.setClosePathNeeded(true);
    const element = renderGraph(parentEl, state, totalDisplayedDuration,
                                "keyframes-easing", graphHelper);
    element.style.opacity = keyframeEasingOpacity;
  }
}

/**
 * Render effect easing graph.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {float} totalDisplayedDuration - Displayable total duration.
 * @param {float} minEffectProgressThreshold - Minimum progress threshold for effect.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderEffectEasingGraph(parentEl, state, totalDisplayedDuration,
                                 minEffectProgressThreshold, graphHelper) {
  graphHelper.setMinProgressThreshold(minEffectProgressThreshold);
  graphHelper.setKeyframes(null);
  graphHelper.setClosePathNeeded(false);
  renderGraph(parentEl, state, totalDisplayedDuration, "effect-easing", graphHelper);
}

/**
 * Render a graph of given parameters.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {float} totalDisplayedDuration - Displayable total duration.
 * @param {String} className - Class name for graph element.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderGraph(parentEl, state, totalDisplayedDuration, className, graphHelper) {
  const graphEl = createSVGNode({
    parent: parentEl,
    nodeType: "g",
    attributes: {
      "class": className,
    }
  });

  // Starting time of main iteration.
  let mainIterationStartTime = 0;
  let iterationStart = state.iterationStart;
  let iterationCount = state.iterationCount ? state.iterationCount : Infinity;

  graphHelper.setFillMode(state.fill);
  graphHelper.setOriginalBehavior(true);

  // Append delay.
  if (state.delay > 0) {
    renderDelay(graphEl, state, graphHelper);
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
    renderFirstIteration(graphEl, state, mainIterationStartTime,
                         firstSectionCount, graphHelper);
  }

  if (iterationCount === Infinity) {
    // If the animation repeats infinitely,
    // we fill the remaining area with iteration paths.
    renderInfinity(graphEl, state, mainIterationStartTime,
                   firstSectionCount, totalDisplayedDuration, graphHelper);
  } else {
    // Otherwise, we show remaining iterations, endDelay and fill.

    // Append forwards fill-mode.
    if (state.fill === "both" || state.fill === "forwards") {
      renderForwardsFill(graphEl, state, mainIterationStartTime,
                         iterationCount, totalDisplayedDuration, graphHelper);
    }

    // Append middle section of iterations.
    // e.g.
    // if { iterationStart: 0.25, iterations: 3 }, middleSectionCount is 2.
    const middleSectionCount =
      Math.floor(iterationCount - firstSectionCount);
    renderMiddleIterations(graphEl, state, mainIterationStartTime,
                           firstSectionCount, middleSectionCount, graphHelper);

    // Append last section of iterations, if there is remaining iteration.
    // e.g.
    // if { iterationStart: 0.25, iterations: 3 }, lastSectionCount is 0.25.
    const lastSectionCount =
      iterationCount - middleSectionCount - firstSectionCount;
    if (lastSectionCount) {
      renderLastIteration(graphEl, state, mainIterationStartTime,
                          firstSectionCount, middleSectionCount,
                          lastSectionCount, graphHelper);
    }

    // Append endDelay.
    if (state.endDelay > 0) {
      renderEndDelay(graphEl, state,
                     mainIterationStartTime, iterationCount, graphHelper);
    }
  }

  // Append negative delay (which overlap the animation).
  if (state.delay < 0) {
    graphHelper.setFillMode("both");
    graphHelper.setOriginalBehavior(false);
    renderNegativeDelayHiddenProgress(graphEl, state, graphHelper);
  }
  // Append negative endDelay (which overlap the animation).
  if (state.iterationCount && state.endDelay < 0) {
    graphHelper.setFillMode("both");
    graphHelper.setOriginalBehavior(false);
    renderNegativeEndDelayHiddenProgress(graphEl, state, graphHelper);
  }

  return graphEl;
}

/**
 * Render delay section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderDelay(parentEl, state, graphHelper) {
  const startSegment = graphHelper.getSegment(0);
  const endSegment = { x: state.delay, y: startSegment.y };
  graphHelper.appendPathElement(parentEl, [startSegment, endSegment], "delay-path");
}

/**
 * Render first iteration section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderFirstIteration(parentEl, state, mainIterationStartTime,
                              firstSectionCount, graphHelper) {
  const startTime = mainIterationStartTime;
  const endTime = startTime + firstSectionCount * state.duration;
  const segments = graphHelper.createPathSegments(startTime, endTime);
  graphHelper.appendPathElement(parentEl, segments, "iteration-path");
}

/**
 * Render middle iterations section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} middleSectionCount - Iteration count of middle section.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderMiddleIterations(parentEl, state, mainIterationStartTime,
                                firstSectionCount, middleSectionCount,
                                graphHelper) {
  const offset = mainIterationStartTime + firstSectionCount * state.duration;
  for (let i = 0; i < middleSectionCount; i++) {
    // Get the path segments of each iteration.
    const startTime = offset + i * state.duration;
    const endTime = startTime + state.duration;
    const segments = graphHelper.createPathSegments(startTime, endTime);
    graphHelper.appendPathElement(parentEl, segments, "iteration-path");
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
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderLastIteration(parentEl, state, mainIterationStartTime,
                             firstSectionCount, middleSectionCount,
                             lastSectionCount, graphHelper) {
  const startTime = mainIterationStartTime +
                      (firstSectionCount + middleSectionCount) * state.duration;
  const endTime = startTime + lastSectionCount * state.duration;
  const segments = graphHelper.createPathSegments(startTime, endTime);
  graphHelper.appendPathElement(parentEl, segments, "iteration-path");
}

/**
 * Render Infinity iterations.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} firstSectionCount - Iteration count of first section.
 * @param {Number} totalDuration - Displayed max duration.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderInfinity(parentEl, state, mainIterationStartTime,
                        firstSectionCount, totalDuration, graphHelper) {
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
    graphHelper.createPathSegments(firstStartTime, firstEndTime);
  graphHelper.appendPathElement(parentEl, firstSegments, "iteration-path infinity");

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
    graphHelper.appendPathElement(parentEl, segments, "iteration-path infinity copied");
  }
}

/**
 * Render endDelay section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} iterationCount - Whole iteration count.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderEndDelay(parentEl, state,
                        mainIterationStartTime, iterationCount, graphHelper) {
  const startTime = mainIterationStartTime + iterationCount * state.duration;
  const startSegment = graphHelper.getSegment(startTime);
  const endSegment = { x: startTime + state.endDelay, y: startSegment.y };
  graphHelper.appendPathElement(parentEl, [startSegment, endSegment], "enddelay-path");
}

/**
 * Render forwards fill section.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Number} mainIterationStartTime - Starting time of main iteration.
 * @param {Number} iterationCount - Whole iteration count.
 * @param {Number} totalDuration - Displayed max duration.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderForwardsFill(parentEl, state, mainIterationStartTime,
                            iterationCount, totalDuration, graphHelper) {
  const startTime = mainIterationStartTime + iterationCount * state.duration +
                      (state.endDelay > 0 ? state.endDelay : 0);
  const startSegment = graphHelper.getSegment(startTime);
  const endSegment = { x: totalDuration, y: startSegment.y };
  graphHelper.appendPathElement(parentEl, [startSegment, endSegment],
                                "fill-forwards-path");
}

/**
 * Render hidden progress of negative delay.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderNegativeDelayHiddenProgress(parentEl, state, graphHelper) {
  const startTime = state.delay;
  const endTime = 0;
  const segments =
    graphHelper.createPathSegments(startTime, endTime);
  graphHelper.appendPathElement(parentEl, segments, "delay-path negative");
}

/**
 * Render hidden progress of negative endDelay.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Object} state - State of animation.
 * @param {Object} graphHelper - SummaryGraphHelper.
 */
function renderNegativeEndDelayHiddenProgress(parentEl, state, graphHelper) {
  const endTime = state.delay + state.iterationCount * state.duration;
  const startTime = endTime + state.endDelay;
  const segments = graphHelper.createPathSegments(startTime, endTime);
  graphHelper.appendPathElement(parentEl, segments, "enddelay-path negative");
}

/**
 * Create new keyframes object which has only offset and easing.
 * Also, the returned value has no duplication.
 * @param {Object} tracks - The value of AnimationsTimeline.getTracks().
 * @return {Array} keyframes list.
 */
function getOffsetAndEasingOnlyKeyframesList(tracks) {
  return Object.keys(tracks).reduce((result, name) => {
    const track = tracks[name];
    const exists = result.find(keyframes => {
      if (track.length !== keyframes.length) {
        return false;
      }
      for (let i = 0; i < track.length; i++) {
        const keyframe1 = track[i];
        const keyframe2 = keyframes[i];
        if (keyframe1.offset !== keyframe2.offset ||
            keyframe1.easing !== keyframe2.easing) {
          return false;
        }
      }
      return true;
    });
    if (!exists) {
      const keyframes = track.map(keyframe => {
        return { offset: keyframe.offset, easing: keyframe.easing };
      });
      result.push(keyframes);
    }
    return result;
  }, []);
}
