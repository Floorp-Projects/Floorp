/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// BOUND_EXCLUDING_TIME should be less than 1ms and is used to exclude start
// and end bounds when dividing duration in createPathSegments.
const BOUND_EXCLUDING_TIME = 0.001;
// We define default graph height since if the height of viewport in SVG is
// too small (e.g. 1), vector-effect may not be able to calculate correctly.
const DEFAULT_GRAPH_HEIGHT = 100;
// Default animation duration for keyframes graph.
const DEFAULT_KEYFRAMES_GRAPH_DURATION = 1000;
// DEFAULT_MIN_PROGRESS_THRESHOLD shoud be between more than 0 to 1.
const DEFAULT_MIN_PROGRESS_THRESHOLD = 0.1;
// In the createPathSegments function, an animation duration is divided by
// DEFAULT_DURATION_RESOLUTION in order to draw the way the animation progresses.
// But depending on the timing-function, we may be not able to make the graph
// smoothly progress if this resolution is not high enough.
// So, if the difference of animation progress between 2 divisions is more than
// DEFAULT_MIN_PROGRESS_THRESHOLD * DEFAULT_GRAPH_HEIGHT, then createPathSegments
// re-divides by DEFAULT_DURATION_RESOLUTION.
// DEFAULT_DURATION_RESOLUTION shoud be integer and more than 2.
const DEFAULT_DURATION_RESOLUTION = 4;
// Stroke width for easing hint.
const DEFAULT_EASING_HINT_STROKE_WIDTH = 5;

/**
 * The helper class for creating summary graph.
 */
class SummaryGraphHelper {
  /**
   * Constructor.
   *
   * @param {Object} state
   *        State of animation.
   * @param {Array} keyframes
   *        Array of keyframe.
   * @param {Number} totalDuration
   *        Total displayable duration.
   * @param {Number} minSegmentDuration
   *        Minimum segment duration.
   * @param {Function} getValueFunc
   *        Which returns graph value of given time.
   *        The function should return a number value between 0 - 1.
   *        e.g. time => { return 1.0 };
   * @param {Function} toPathStringFunc
   *        Which returns a path string for 'd' attribute for <path> from given segments.
   */
  constructor(state, keyframes, totalDuration, minSegmentDuration,
              getValueFunc, toPathStringFunc) {
    this.totalDuration = totalDuration;
    this.minSegmentDuration = minSegmentDuration;
    this.minProgressThreshold =
      getPreferredProgressThreshold(state, keyframes) * DEFAULT_GRAPH_HEIGHT;
    this.durationResolution = getPreferredDurationResolution(keyframes);
    this.getValue = getValueFunc;
    this.toPathString = toPathStringFunc;

    this.getSegment = this.getSegment.bind(this);
  }

  /**
   * Create the path segments from given parameters.
   *
   * @param {Number} startTime
   *        Starting time of animation.
   * @param {Number} endTime
   *        Ending time of animation.
   * @return {Array}
   *         Array of path segment.
   *         e.g.[{x: {Number} time, y: {Number} progress}, ...]
   */
  createPathSegments(startTime, endTime) {
    return createPathSegments(startTime, endTime,
                              this.minSegmentDuration, this.minProgressThreshold,
                              this.durationResolution, this.getSegment);
  }

  /**
   * Return a coordinate as a graph segment at given time.
   *
   * @param {Number} time
   * @return {Object}
   *         { x: Number, y: Number }
   */
  getSegment(time) {
    const value = this.getValue(time);
    return { x: time, y: value * DEFAULT_GRAPH_HEIGHT };
  }
}

/**
 * Create the path segments from given parameters.
 *
 * @param {Number} startTime
 *        Starting time of animation.
 * @param {Number} endTime
 *        Ending time of animation.
 * @param {Number} minSegmentDuration
 *        Minimum segment duration.
 * @param {Number} minProgressThreshold
 *        Minimum progress threshold.
 * @param {Number} resolution
 *        Duration resolution for first time.
 * @param {Function} getSegment
 *        A function that calculate the graph segment.
 * @return {Array}
 *         Array of path segment.
 *         e.g.[{x: {Number} time, y: {Number} progress}, ...]
 */
function createPathSegments(startTime, endTime, minSegmentDuration,
                            minProgressThreshold, resolution, getSegment) {
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
  // See the definition of DEFAULT_DURATION_RESOLUTION for more information about this.
  const interval = (endTime - startTime) / resolution;
  for (let index = 1; index <= resolution; index++) {
    // Create a segment for this interval.
    const currentSegment = getSegment(startTime + index * interval);

    // If the distance between the Y coordinate (the animation's progress) of
    // the previous segment and the Y coordinate of the current segment is too
    // large, then recurse with a smaller duration to get more details
    // in the graph.
    if (Math.abs(currentSegment.y - previousSegment.y) > minProgressThreshold) {
      // Divide the current interval (excluding start and end bounds
      // by adding/subtracting BOUND_EXCLUDING_TIME).
      const nextStartTime = previousSegment.x + BOUND_EXCLUDING_TIME;
      const nextEndTime = currentSegment.x - BOUND_EXCLUDING_TIME;
      const segments =
        createPathSegments(nextStartTime, nextEndTime, minSegmentDuration,
                           minProgressThreshold, DEFAULT_DURATION_RESOLUTION, getSegment);
      pathSegments = pathSegments.concat(segments);
    }

    pathSegments.push(currentSegment);
    previousSegment = currentSegment;
  }

  return pathSegments;
}

/**
 * Return preferred duration resolution.
 * This corresponds to narrow interval keyframe offset.
 *
 * @param {Array} keyframes
 *        Array of keyframe.
 * @return {Number}
 *         Preferred duration resolution.
 */
function getPreferredDurationResolution(keyframes) {
  if (!keyframes) {
    return DEFAULT_DURATION_RESOLUTION;
  }

  let durationResolution = DEFAULT_DURATION_RESOLUTION;
  let previousOffset = 0;
  for (const keyframe of keyframes) {
    if (previousOffset && previousOffset != keyframe.offset) {
      const interval = keyframe.offset - previousOffset;
      durationResolution = Math.max(durationResolution, Math.ceil(1 / interval));
    }
    previousOffset = keyframe.offset;
  }

  return durationResolution;
}

/**
 * Return preferred progress threshold to render summary graph.
 *
 * @param {Object} state
 *        State of animation.
 * @param {Array} keyframes
 *        Array of keyframe.
 * @return {float}
 *         Preferred threshold.
 */
function getPreferredProgressThreshold(state, keyframes) {
  let threshold = DEFAULT_MIN_PROGRESS_THRESHOLD;
  let stepsOrFrames;

  if ((stepsOrFrames = getStepsOrFramesCount(state.easing))) {
    threshold = Math.min(threshold, (1 / (stepsOrFrames + 1)));
  }

  if (!keyframes) {
    return threshold;
  }

  threshold = Math.min(threshold, getPreferredProgressThresholdByKeyframes(keyframes));

  return threshold;
}

/**
 * Return preferred progress threshold by keyframes.
 *
 * @param {Array} keyframes
 *        Array of keyframe.
 * @return {float}
 *         Preferred threshold.
 */
function getPreferredProgressThresholdByKeyframes(keyframes) {
  let threshold = DEFAULT_MIN_PROGRESS_THRESHOLD;
  let stepsOrFrames;

  for (let i = 0; i < keyframes.length - 1; i++) {
    const keyframe = keyframes[i];

    if (!keyframe.easing) {
      continue;
    }

    if ((stepsOrFrames = getStepsOrFramesCount(keyframe.easing))) {
      const nextKeyframe = keyframes[i + 1];
      threshold =
        Math.min(threshold,
                 1 / (stepsOrFrames + 1) * (nextKeyframe.offset - keyframe.offset));
    }
  }

  return threshold;
}

function getStepsOrFramesCount(easing) {
  const stepsOrFramesFunction = easing.match(/(steps|frames)\((\d+)/);
  return stepsOrFramesFunction ? parseInt(stepsOrFramesFunction[2], 10) : 0;
}

/**
 * Return path string for 'd' attribute for <path> from given segments.
 *
 * @param {Array} segments
 *        e.g. [{ x: 100, y: 0 }, { x: 200, y: 1 }]
 * @return {String}
 *         Path string.
 *         e.g. "L100,0 L200,1"
 */
function toPathString(segments) {
  let pathString = "";
  segments.forEach(segment => {
    pathString += `L${ segment.x },${ segment.y } `;
  });
  return pathString;
}

exports.createPathSegments = createPathSegments;
exports.DEFAULT_DURATION_RESOLUTION = DEFAULT_DURATION_RESOLUTION;
exports.DEFAULT_EASING_HINT_STROKE_WIDTH = DEFAULT_EASING_HINT_STROKE_WIDTH;
exports.DEFAULT_GRAPH_HEIGHT = DEFAULT_GRAPH_HEIGHT;
exports.DEFAULT_KEYFRAMES_GRAPH_DURATION = DEFAULT_KEYFRAMES_GRAPH_DURATION;
exports.getPreferredProgressThresholdByKeyframes =
  getPreferredProgressThresholdByKeyframes;
exports.SummaryGraphHelper = SummaryGraphHelper;
exports.toPathString = toPathString;
