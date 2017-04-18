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
  this.animation = this.targetEl.animate(this.keyframes, effectTiming);
  this.animation.pause();
  this.valueHelperFunction = this.getValueHelperFunction();
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
      if (!discreteValues.includes(keyframe.value)) {
        discreteValues.push(keyframe.value);
      }
    });
    return value => {
      return discreteValues.indexOf(value) / (discreteValues.length - 1);
    };
  },

  /**
   * Create the path segments from given parameters.
   * @param {Number} startTime - Starting time of animation.
   * @param {Number} endTime - Ending time of animation.
   * @param {Number} minSegmentDuration - Minimum segment duration.
   * @param {Number} minProgressThreshold - Minimum progress threshold.
   * @return {Array} path segments -
   *                 [{x: {Number} time, y: {Number} progress}, ...]
   */
  createPathSegments: function (startTime, endTime,
                                minSegmentDuration, minProgressThreshold) {
    return !this.valueHelperFunction
           ? createKeyframesPathSegments(endTime - startTime, this.devtoolsKeyframes)
           : createPathSegments(startTime, endTime,
                                minSegmentDuration, minProgressThreshold, this);
  },
};

exports.ProgressGraphHelper = ProgressGraphHelper;

/**
 * Create the path segments from given parameters.
 * @param {Number} startTime - Starting time of animation.
 * @param {Number} endTime - Ending time of animation.
 * @param {Number} minSegmentDuration - Minimum segment duration.
 * @param {Number} minProgressThreshold - Minimum progress threshold.
 * @param {Object} segmentHelper
 * - getSegment(time): Helper function that, given a time,
 *                     will calculate the animation progress.
 * @return {Array} path segments -
 *                 [{x: {Number} time, y: {Number} progress}, ...]
 */
function createPathSegments(startTime, endTime, minSegmentDuration,
                            minProgressThreshold, segmentHelper) {
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
  const interval = (endTime - startTime) / DURATION_RESOLUTION;
  for (let index = 1; index <= DURATION_RESOLUTION; index++) {
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
exports.createPathSegments = createPathSegments;

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
  for (let i = 0; i < pathSegments.length; i++) {
    const pathSegment = pathSegments[i];
    if (!pathSegment.easing || pathSegment.easing === "linear") {
      path += createLinePathString(pathSegment);
      continue;
    }

    if (i + 1 === pathSegments.length) {
      // We already create steps or cubic-bezier path string in previous.
      break;
    }

    const nextPathSegment = pathSegments[i + 1];
    path += pathSegment.easing.startsWith("steps")
            ? createStepsPathString(pathSegment, nextPathSegment)
            : createCubicBezierPathString(pathSegment, nextPathSegment);
  }
  path += ` L${ pathSegments[pathSegments.length - 1].x },0 Z`;
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
exports.appendPathElement = appendPathElement;

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
