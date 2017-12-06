/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {createSVGNode, getJsPropertyName} =
  require("devtools/client/animationinspector/utils");
const {colorUtils} = require("devtools/shared/css/color.js");
const {parseTimingFunction} = require("devtools/client/shared/widgets/CubicBezierWidget");

// In the createPathSegments function, an animation duration is divided by
// DURATION_RESOLUTION in order to draw the way the animation progresses.
// But depending on the timing-function, we may be not able to make the graph
// smoothly progress if this resolution is not high enough.
// So, if the difference of animation progress between 2 divisions is more than
// DEFAULT_MIN_PROGRESS_THRESHOLD, then createPathSegments re-divides
// by DURATION_RESOLUTION.
// DURATION_RESOLUTION shoud be integer and more than 2.
const DURATION_RESOLUTION = 4;
// DEFAULT_MIN_PROGRESS_THRESHOLD shoud be between more than 0 to 1.
const DEFAULT_MIN_PROGRESS_THRESHOLD = 0.1;
exports.DEFAULT_MIN_PROGRESS_THRESHOLD = DEFAULT_MIN_PROGRESS_THRESHOLD;
// BOUND_EXCLUDING_TIME should be less than 1ms and is used to exclude start
// and end bounds when dividing  duration in createPathSegments.
const BOUND_EXCLUDING_TIME = 0.001;

/**
 * This helper return the segment coordinates and style for property graph,
 * also return the graph type.
 * Parameters of constructor are below.
 * @param {Window} win - window object to animate.
 * @param {String} propertyCSSName - CSS property name (e.g. background-color).
 * @param {String} animationType - Animation type of CSS property.
 * @param {Object} keyframes - AnimationInspector's keyframes object.
 * @param {float}  duration - Duration of animation.
 */
function ProgressGraphHelper(win, propertyCSSName, animationType, keyframes, duration) {
  this.win = win;
  const doc = this.win.document;
  this.targetEl = doc.createElement("div");
  doc.documentElement.appendChild(this.targetEl);

  this.propertyCSSName = propertyCSSName;
  this.propertyJSName = getJsPropertyName(this.propertyCSSName);
  this.animationType = animationType;

  // Create keyframe object to make dummy animation.
  const keyframesObject = keyframes.map(keyframe => {
    const keyframeObject = Object.assign({}, keyframe);
    keyframeObject[this.propertyJSName] = keyframe.value;
    return keyframeObject;
  });

  // Create effect timing object to make dummy animation.
  const effectTiming = {
    duration: duration,
    fill: "forwards"
  };

  this.keyframes = keyframesObject;
  this.devtoolsKeyframes = keyframes;
  this.valueHelperFunction = this.getValueHelperFunction();
  this.animation = this.targetEl.animate(this.keyframes, effectTiming);
  this.animation.pause();
}

ProgressGraphHelper.prototype = {
  /**
   * Destory this object.
   */
  destroy: function () {
    this.targetEl.remove();
    this.animation.cancel();

    this.targetEl = null;
    this.animation = null;
    this.valueHelperFunction = null;
    this.propertyCSSName = null;
    this.propertyJSName = null;
    this.animationType = null;
    this.keyframes = null;
    this.win = null;
  },

  /**
   * Return animation duration.
   * @return {Number} duration
   */
  getDuration: function () {
    return this.animation.effect.timing.duration;
  },

  /**
   * Return animation's keyframe.
   * @return {Object} keyframe
   */
  getKeyframes: function () {
    return this.keyframes;
  },

  /**
   * Return graph type.
   * @return {String} if property is 'opacity' or 'transform', return that value.
   *                  Otherwise, return given animation type in constructor.
   */
  getGraphType: function () {
    return (this.propertyJSName === "opacity" || this.propertyJSName === "transform")
           ? this.propertyJSName : this.animationType;
  },

  /**
   * Return a segment in graph by given the time.
   * @return {Object} Computed result which has follwing values.
   * - x: x value of graph (float)
   * - y: y value of graph (float between 0 - 1)
   * - style: the computed style value of the property at the time
   */
  getSegment: function (time) {
    this.animation.currentTime = time;
    const style = this.win.getComputedStyle(this.targetEl)[this.propertyJSName];
    const value = this.valueHelperFunction(style);
    return { x: time, y: value, style: style };
  },

  /**
   * Get a value helper function which calculates the value of Y axis by animation type.
   * @return {function} ValueHelperFunction returns float value of Y axis
   *                    from given progress and style (e.g. rgb(0, 0, 0))
   */
  getValueHelperFunction: function () {
    switch (this.animationType) {
      case "none": {
        return () => 1;
      }
      case "float": {
        return this.getFloatValueHelperFunction();
      }
      case "coord": {
        return this.getCoordinateValueHelperFunction();
      }
      case "color": {
        return this.getColorValueHelperFunction();
      }
      case "discrete": {
        return this.getDiscreteValueHelperFunction();
      }
    }
    return null;
  },

  /**
   * Return value helper function of animation type 'float'.
   * @param {Object} keyframes - This object shoud be same as
   *                             the parameter of getGraphHelper.
   * @return {function} ValueHelperFunction returns float value of Y axis
   *                    from given float (e.g. 1.0, 0.5 and so on)
   */
  getFloatValueHelperFunction: function () {
    let maxValue = 0;
    let minValue = Infinity;
    this.keyframes.forEach(keyframe => {
      maxValue = Math.max(maxValue, keyframe.value);
      minValue = Math.min(minValue, keyframe.value);
    });
    const distance = maxValue - minValue;
    return value => {
      return (value - minValue) / distance;
    };
  },

  /**
   * Return value helper function of animation type 'coord'.
   * @return {function} ValueHelperFunction returns float value of Y axis
   *                    from given style (e.g. 100px)
   */
  getCoordinateValueHelperFunction: function () {
    let maxValue = 0;
    let minValue = Infinity;
    for (let i = 0, n = this.keyframes.length; i < n; i++) {
      if (this.keyframes[i].value.match(/calc/)) {
        return null;
      }
      const value = parseFloat(this.keyframes[i].value);
      minValue = Math.min(minValue, value);
      maxValue = Math.max(maxValue, value);
    }
    const distance = maxValue - minValue;
    return value => {
      return (parseFloat(value) - minValue) / distance;
    };
  },

  /**
   * Return value helper function of animation type 'color'.
   * @param {Object} keyframes - This object shoud be same as
   *                             the parameter of getGraphHelper.
   * @return {function} ValueHelperFunction returns float value of Y axis
   *                    from given color (e.g. rgb(0, 0, 0))
   */
  getColorValueHelperFunction: function () {
    const maxObject = { distance: 0 };
    for (let i = 0; i < this.keyframes.length - 1; i++) {
      const value1 = getRGBA(this.keyframes[i].value);
      for (let j = i + 1; j < this.keyframes.length; j++) {
        const value2 = getRGBA(this.keyframes[j].value);
        const distance = getRGBADistance(value1, value2);
        if (maxObject.distance >= distance) {
          continue;
        }
        maxObject.distance = distance;
        maxObject.value1 = value1;
        maxObject.value2 = value2;
      }
    }
    const baseValue =
      maxObject.value1 < maxObject.value2 ? maxObject.value1 : maxObject.value2;

    return value => {
      const colorValue = getRGBA(value);
      return getRGBADistance(baseValue, colorValue) / maxObject.distance;
    };
  },

  /**
   * Return value helper function of animation type 'discrete'.
   * @return {function} ValueHelperFunction returns float value of Y axis
   *                    from given style (e.g. center)
   */
  getDiscreteValueHelperFunction: function () {
    const discreteValues = [];
    this.keyframes.forEach(keyframe => {
      // Set style once since the computed value may differ from specified keyframe value.
      this.targetEl.style[this.propertyJSName] = keyframe.value;
      const style = this.win.getComputedStyle(this.targetEl)[this.propertyJSName];
      if (!discreteValues.includes(style)) {
        discreteValues.push(style);
      }
    });
    this.targetEl.style[this.propertyJSName] = "unset";

    return value => {
      return discreteValues.indexOf(value) / (discreteValues.length - 1);
    };
  },

  /**
   * Create the path segments from given parameters.
   *
   * @param {Number} duration - Duration of animation.
   * @param {Number} minSegmentDuration - Minimum segment duration.
   * @param {Number} minProgressThreshold - Minimum progress threshold.
   * @return {Array} path segments -
   *                 [{x: {Number} time, y: {Number} progress}, ...]
   */
  createPathSegments: function (duration, minSegmentDuration, minProgressThreshold) {
    if (!this.valueHelperFunction) {
      return createKeyframesPathSegments(duration, this.devtoolsKeyframes);
    }

    const segments = [];

    for (let i = 0; i < this.devtoolsKeyframes.length - 1; i++) {
      const startKeyframe = this.devtoolsKeyframes[i];
      const endKeyframe = this.devtoolsKeyframes[i + 1];

      let threshold = getPreferredProgressThreshold(startKeyframe.easing);
      if (threshold !== DEFAULT_MIN_PROGRESS_THRESHOLD) {
        // We should consider the keyframe's duration.
        threshold *= (endKeyframe.offset - startKeyframe.offset);
      }

      const startTime = startKeyframe.offset * duration;
      const endTime = endKeyframe.offset * duration;

      segments.push(...createPathSegments(startTime, endTime,
                                          minSegmentDuration, threshold, this));
    }

    return segments;
  },

  /**
   * Append path element as shape. Also, this method appends two segment
   * that are {start x, 0} and {end x, 0} to make shape.
   * @param {Element} parentEl - Parent element of this appended path element.
   * @param {Array} pathSegments - Path segments. Please see createPathSegments.
   * @param {String} cls - Class name.
   * @return {Element} path element.
   */
  appendShapePath: function (parentEl, pathSegments, cls) {
    return appendShapePath(parentEl, pathSegments, cls);
  },

  /**
   * Append path element as line.
   * @param {Element} parentEl - Parent element of this appended path element.
   * @param {Array} pathSegments - Path segments. Please see createPathSegments.
   * @param {String} cls - Class name.
   * @return {Element} path element.
   */
  appendLinePath: function (parentEl, pathSegments, cls) {
    const isClosePathNeeded = false;
    return appendPathElement(parentEl, pathSegments, cls, isClosePathNeeded);
  },
};

exports.ProgressGraphHelper = ProgressGraphHelper;

/**
 * This class is used for creating the summary graph in animation-timeline.
 * The shape of the graph can be changed by using the following methods:
 * setKeyframes:
 *   If null, the shape is by computed timing progress.
 *   Otherwise, by computed style of 'opacity' to combine effect easing and
 *   keyframe's easing.
 * setFillMode:
 *   Animation fill-mode (e.g. "none", "backwards", "forwards" or "both")
 * setClosePathNeeded:
 *   If true, appendShapePath make the last segment of <path> element to
 *   "close" segment("Z").
 *   Therefore, if don't need under-line of graph, please set false.
 * setOriginalBehavior:
 *   In Animation::SetCurrentTime spec, even if current time of animation is over
 *   the endTime, the progress is changed. Likewise, in case the time is less than 0.
 *   If set true, prevent the time to make the same animation behavior as the original.
 * setMinProgressThreshold:
 *   SummaryGraphHelper searches and creates the summary graph until the progress
 *   distance is less than this minProgressThreshold.
 *   So, while setting a low threshold produces a smooth graph,
 *   it will have an effect on performance.
 * @param {Object} win - window object.
 * @param {Object} state - animation state.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 */
function SummaryGraphHelper(win, state, minSegmentDuration) {
  this.win = win;
  const doc = this.win.document;
  this.targetEl = doc.createElement("div");
  doc.documentElement.appendChild(this.targetEl);

  const effectTiming = Object.assign({}, state, {
    iterations: state.iterationCount ? state.iterationCount : Infinity
  });
  this.animation = this.targetEl.animate(null, effectTiming);
  this.animation.pause();
  this.endTime = this.animation.effect.getComputedTiming().endTime;

  this.minSegmentDuration = minSegmentDuration;
  this.minProgressThreshold = DEFAULT_MIN_PROGRESS_THRESHOLD;
}

SummaryGraphHelper.prototype = {
  /**
   * Destory this object.
   */
  destroy: function () {
    this.animation.cancel();
    this.targetEl.remove();
    this.targetEl = null;
    this.animation = null;
    this.win = null;
  },

  /*
   * Set keyframes to shape graph by computed style. This method creates new keyframe
   * object using only offset and easing of given keyframes.
   * Also, allows null value. In case of null, this graph helper shapes graph using
   * computed timing progress.
   * @param {Object} keyframes - Should have offset and easing, or null.
   */
  setKeyframes: function (keyframes) {
    let frames = null;
    // We need to change the duration resolution in case of interval of keyframes offset
    // was narrow.
    let durationResolution = DURATION_RESOLUTION;

    if (keyframes) {
      let previousOffset = 0;

      // Create new keyframes for opacity as computed style.
      // The reason why we use computed value instead of computed timing progress is to
      // include the easing in keyframes as well. Although the computed timing progress
      // is not affected by the easing in keyframes at all, computed value reflects that.
      frames = keyframes.map(keyframe => {
        if (previousOffset) {
          const interval = keyframe.offset - previousOffset;
          durationResolution = Math.max(durationResolution, Math.ceil(1 / interval));
        }
        previousOffset = keyframe.offset;

        return {
          opacity: keyframe.offset,
          offset: keyframe.offset,
          easing: keyframe.easing
        };
      });

      // Set the underlying opacity to zero so that if we sample the animation's output
      // during the delay phase and it is not filling backwards, we get zero.
      this.targetEl.style.opacity = 0;
    }

    this.durationResolution = durationResolution;
    this.animation.effect.setKeyframes(frames);
    this.hasFrames = !!frames;
  },

  /*
   * Set animation behavior.
   * In Animation::SetCurrentTime spec, even if current time of animation is over
   * endTime, the progress is changed. Likewise, in case the time is less than 0.
   * If set true, we prevent the time to make the same animation behavior as the original.
   * @param {bool} isOriginalBehavior - true: original behavior
   *                                    false: according to spec.
   */
  setOriginalBehavior: function (isOriginalBehavior) {
    this.isOriginalBehavior = isOriginalBehavior;
  },

  /**
   * Set animation fill mode.
   * @param {String} fill - "both", "forwards", "backwards" or "both"
   */
  setFillMode: function (fill) {
    this.animation.effect.timing.fill = fill;
  },

  /**
   * Set true if need to close path in appendShapePath.
   * @param {bool} isClosePathNeeded - true: close, false: open.
   */
  setClosePathNeeded: function (isClosePathNeeded) {
    this.isClosePathNeeded = isClosePathNeeded;
  },

  /**
   * SummaryGraphHelper searches and creates the summary graph untill the progress
   * distance is less than this minProgressThreshold.
   */
  setMinProgressThreshold: function (minProgressThreshold) {
    this.minProgressThreshold = minProgressThreshold;
  },

  /**
   * Return a segment in graph by given the time.
   * @return {Object} Computed result which has follwing values.
   * - x: x value of graph (float)
   * - y: y value of graph (float between 0 - 1)
   */
  getSegment: function (time) {
    if (this.isOriginalBehavior) {
      // If the given time is less than 0, returned progress is 0.
      if (time < 0) {
        return { x: time, y: 0 };
      }
      // Avoid to apply over endTime.
      this.animation.currentTime = time < this.endTime ? time : this.endTime;
    } else {
      this.animation.currentTime = time;
    }
    const value = this.hasFrames ? this.getOpacityValue() : this.getProgressValue();
    return { x: time, y: value };
  },

  /**
   * Create the path segments from given parameters.
   * @param {Number} startTime - Starting time of animation.
   * @param {Number} endTime - Ending time of animation.
   * @return {Array} path segments -
   *                 [{x: {Number} time, y: {Number} progress}, ...]
   */
  createPathSegments: function (startTime, endTime) {
    return createPathSegments(startTime, endTime, this.minSegmentDuration,
                              this.minProgressThreshold, this, this.durationResolution);
  },

  /**
   * Append path element as shape. Also, this method appends two segment
   * that are {start x, 0} and {end x, 0} to make shape.
   * @param {Element} parentEl - Parent element of this appended path element.
   * @param {Array} pathSegments - Path segments. Please see createPathSegments.
   * @param {String} cls - Class name.
   * @return {Element} path element.
   */
  appendShapePath: function (parentEl, pathSegments, cls) {
    return appendShapePath(parentEl, pathSegments, cls, this.isClosePathNeeded);
  },

  /**
   * Return current computed timing progress of the animation.
   * @return {float} computed timing progress as float value of Y axis.
   */
  getProgressValue: function () {
    return Math.max(this.animation.effect.getComputedTiming().progress, 0);
  },

  /**
   * Return current computed 'opacity' value of the element which is animating.
   * @return {float} computed timing progress as float value of Y axis.
   */
  getOpacityValue: function () {
    return this.win.getComputedStyle(this.targetEl).opacity;
  }
};

exports.SummaryGraphHelper = SummaryGraphHelper;

/**
 * Create the path segments from given parameters.
 * @param {Number} startTime - Starting time of animation.
 * @param {Number} endTime - Ending time of animation.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper
 * @param {Number} resolution - Duration resolution for first time.
 *                              If null, use DURATION_RESOLUTION.
 * - getSegment(time): Helper function that, given a time,
 *                     will calculate the animation progress.
 * @return {Array} path segments -
 *                 [{x: {Number} time, y: {Number} progress}, ...]
 */
function createPathSegments(startTime, endTime, minSegmentDuration,
                            minProgressThreshold, segmentHelper,
                            resolution = DURATION_RESOLUTION) {
  // If the duration is too short, early return.
  if (endTime - startTime < minSegmentDuration) {
    return [segmentHelper.getSegment(startTime),
            segmentHelper.getSegment(endTime)];
  }

  // Otherwise, start creating segments.
  let pathSegments = [];

  // Append the segment for the startTime position.
  const startTimeSegment = segmentHelper.getSegment(startTime);
  pathSegments.push(startTimeSegment);
  let previousSegment = startTimeSegment;

  // Split the duration in equal intervals, and iterate over them.
  // See the definition of DURATION_RESOLUTION for more information about this.
  const interval = (endTime - startTime) / resolution;
  for (let index = 1; index <= resolution; index++) {
    // Create a segment for this interval.
    const currentSegment =
      segmentHelper.getSegment(startTime + index * interval);

    // If the distance between the Y coordinate (the animation's progress) of
    // the previous segment and the Y coordinate of the current segment is too
    // large, then recurse with a smaller duration to get more details
    // in the graph.
    if (Math.abs(currentSegment.y - previousSegment.y) > minProgressThreshold) {
      // Divide the current interval (excluding start and end bounds
      // by adding/subtracting BOUND_EXCLUDING_TIME).
      pathSegments = pathSegments.concat(
        createPathSegments(previousSegment.x + BOUND_EXCLUDING_TIME,
                           currentSegment.x - BOUND_EXCLUDING_TIME,
                           minSegmentDuration, minProgressThreshold,
                           segmentHelper));
    }

    pathSegments.push(currentSegment);
    previousSegment = currentSegment;
  }

  return pathSegments;
}

/**
 * Append path element as shape. Also, this method appends two segment
 * that are {start x, 0} and {end x, 0} to make shape.
 * But does not affect given pathSegments.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Array} pathSegments - Path segments. Please see createPathSegments.
 * @param {String} cls - Class name.
 * @param {bool} isClosePathNeeded - Set true if need to close the path. (default true)
 * @return {Element} path element.
 */
function appendShapePath(parentEl, pathSegments, cls, isClosePathNeeded = true) {
  const segments = [
    { x: pathSegments[0].x, y: 0 },
    ...pathSegments,
    { x: pathSegments[pathSegments.length - 1].x, y: 0 }
  ];
  return appendPathElement(parentEl, segments, cls, isClosePathNeeded);
}

/**
 * Append path element.
 * @param {Element} parentEl - Parent element of this appended path element.
 * @param {Array} pathSegments - Path segments. Please see createPathSegments.
 * @param {String} cls - Class name.
 * @param {bool} isClosePathNeeded - Set true if need to close the path.
 * @return {Element} path element.
 */
function appendPathElement(parentEl, pathSegments, cls, isClosePathNeeded) {
  // Create path string.
  let currentSegment = pathSegments[0];
  let path = `M${ currentSegment.x },${ currentSegment.y }`;
  for (let i = 1; i < pathSegments.length; i++) {
    const currentEasing = currentSegment.easing ? currentSegment.easing : "linear";
    const nextSegment = pathSegments[i];
    if (currentEasing === "linear") {
      path += createLinePathString(nextSegment);
    } else if (currentEasing.startsWith("steps")) {
      path += createStepsPathString(currentSegment, nextSegment);
    } else if (currentEasing.startsWith("frames")) {
      path += createFramesPathString(currentSegment, nextSegment);
    } else {
      path += createCubicBezierPathString(currentSegment, nextSegment);
    }
    currentSegment = nextSegment;
  }
  if (isClosePathNeeded) {
    path += " Z";
  }
  // Append and return the path element.
  return createSVGNode({
    parent: parentEl,
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
 * Create the path segments from given keyframes.
 * @param {Number} duration - Duration of animation.
 * @param {Object} Keyframes of devtool's format.
 * @return {Array} path segments -
 *                 [{x: {Number} time, y: {Number} distance,
 *                  easing: {String} keyframe's easing,
 *                  style: {String} keyframe's value}, ...]
 */
function createKeyframesPathSegments(duration, keyframes) {
  return keyframes.map(keyframe => {
    return {
      x: keyframe.offset * duration,
      y: keyframe.distance,
      easing: keyframe.easing,
      style: keyframe.value
    };
  });
}

/**
 * Create a line path string.
 * @param {Object} segment - e.g. { x: 100, y: 1 }
 * @return {String} path string - e.g. "L100,1"
 */
function createLinePathString(segment) {
  return ` L${ segment.x },${ segment.y }`;
}

/**
 * Create a path string to represents a step function.
 * @param {Object} currentSegment - e.g. { x: 0, y: 0, easing: "steps(2)" }
 * @param {Object} nextSegment - e.g. { x: 1, y: 1 }
 * @return {String} path string - e.g. "C 0.25 0.1, 0.25 1, 1 1"
 */
function createStepsPathString(currentSegment, nextSegment) {
  const matches =
    currentSegment.easing.match(/^steps\((\d+)(,\sstart)?\)/);
  const stepNumber = parseInt(matches[1], 10);
  const oneStepX = (nextSegment.x - currentSegment.x) / stepNumber;
  const oneStepY = (nextSegment.y - currentSegment.y) / stepNumber;
  const isStepStart = matches[2];
  const stepOffsetY = isStepStart ? 1 : 0;
  let path = "";
  for (let step = 0; step < stepNumber; step++) {
    const sx = currentSegment.x + step * oneStepX;
    const ex = sx + oneStepX;
    const y = currentSegment.y + (step + stepOffsetY) * oneStepY;
    path += ` L${ sx },${ y } L${ ex },${ y }`;
  }
  if (!isStepStart) {
    path += ` L${ nextSegment.x },${ nextSegment.y }`;
  }
  return path;
}

/**
 * Create a path string to represents a frames function.
 * @param {Object} currentSegment - e.g. { x: 0, y: 0, easing: "frames(2)" }
 * @param {Object} nextSegment - e.g. { x: 1, y: 1 }
 * @return {String} path string - e.g. "C 0.25 0.1, 0.25 1, 1 1"
 */
function createFramesPathString(currentSegment, nextSegment) {
  const matches =
    currentSegment.easing.match(/^frames\((\d+)\)/);
  const framesNumber = parseInt(matches[1], 10);
  const oneFrameX = (nextSegment.x - currentSegment.x) / framesNumber;
  const oneFrameY = (nextSegment.y - currentSegment.y) / (framesNumber - 1);
  let path = "";
  for (let frame = 0; frame < framesNumber; frame++) {
    const sx = currentSegment.x + frame * oneFrameX;
    const ex = sx + oneFrameX;
    const y = currentSegment.y + frame * oneFrameY;
    path += ` L${ sx },${ y } L${ ex },${ y }`;
  }
  return path;
}

/**
 * Create a path string to represents a bezier curve.
 * @param {Object} currentSegment - e.g. { x: 0, y: 0, easing: "ease" }
 * @param {Object} nextSegment - e.g. { x: 1, y: 1 }
 * @return {String} path string - e.g. "C 0.25 0.1, 0.25 1, 1 1"
 */
function createCubicBezierPathString(currentSegment, nextSegment) {
  const controlPoints = parseTimingFunction(currentSegment.easing);
  if (!controlPoints) {
    // Just return line path string since we could not parse this easing.
    return createLinePathString(currentSegment);
  }

  const cp1x = controlPoints[0];
  const cp1y = controlPoints[1];
  const cp2x = controlPoints[2];
  const cp2y = controlPoints[3];

  const diffX = nextSegment.x - currentSegment.x;
  const diffY = nextSegment.y - currentSegment.y;
  let path =
    ` C ${ currentSegment.x + diffX * cp1x } ${ currentSegment.y + diffY * cp1y }`;
  path += `, ${ currentSegment.x + diffX * cp2x } ${ currentSegment.y + diffY * cp2y }`;
  path += `, ${ nextSegment.x } ${ nextSegment.y }`;
  return path;
}

/**
 * Parse given RGBA string.
 * @param {String} colorString - e.g. rgb(0, 0, 0) or rgba(0, 0, 0, 0.5) and so on.
 * @return {Object} RGBA {r: r, g: g, b: b, a: a}.
 */
function getRGBA(colorString) {
  const color = new colorUtils.CssColor(colorString);
  return color.getRGBATuple();
}

/**
 * Return the distance from give two RGBA.
 * @param {Object} rgba1 - RGBA (format is same to getRGBA)
 * @param {Object} rgba2 - RGBA (format is same to getRGBA)
 * @return {float} distance.
 */
function getRGBADistance(rgba1, rgba2) {
  const startA = rgba1.a;
  const startR = rgba1.r * startA;
  const startG = rgba1.g * startA;
  const startB = rgba1.b * startA;
  const endA = rgba2.a;
  const endR = rgba2.r * endA;
  const endG = rgba2.g * endA;
  const endB = rgba2.b * endA;
  const diffA = startA - endA;
  const diffR = startR - endR;
  const diffG = startG - endG;
  const diffB = startB - endB;
  return Math.sqrt(diffA * diffA + diffR * diffR + diffG * diffG + diffB * diffB);
}

/**
 * Return preferred progress threshold for given keyframes.
 * See the documentation of DURATION_RESOLUTION and DEFAULT_MIN_PROGRESS_THRESHOLD
 * for more information regarding this.
 * @param {Array} keyframes - keyframes
 * @return {float} - preferred progress threshold.
 */
function getPreferredKeyframesProgressThreshold(keyframes) {
  let minProgressTreshold = DEFAULT_MIN_PROGRESS_THRESHOLD;
  for (let i = 0; i < keyframes.length - 1; i++) {
    const keyframe = keyframes[i];
    if (!keyframe.easing) {
      continue;
    }
    let keyframeProgressThreshold = getPreferredProgressThreshold(keyframe.easing);
    if (keyframeProgressThreshold !== DEFAULT_MIN_PROGRESS_THRESHOLD) {
      // We should consider the keyframe's duration.
      keyframeProgressThreshold *=
        (keyframes[i + 1].offset - keyframe.offset);
    }
    minProgressTreshold = Math.min(keyframeProgressThreshold, minProgressTreshold);
  }
  return minProgressTreshold;
}
exports.getPreferredKeyframesProgressThreshold = getPreferredKeyframesProgressThreshold;

/**
 * Return preferred progress threshold to render summary graph.
 * @param {String} - easing e.g. steps(2), linear and so on.
 * @return {float} - preferred threshold.
 */
function getPreferredProgressThreshold(easing) {
  const stepOrFramesFunction = easing.match(/(steps|frames)\((\d+)/);
  return stepOrFramesFunction
       ? 1 / (parseInt(stepOrFramesFunction[2], 10) + 1)
       : DEFAULT_MIN_PROGRESS_THRESHOLD;
}
exports.getPreferredProgressThreshold = getPreferredProgressThreshold;
