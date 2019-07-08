/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { OS } = require("resource://gre/modules/osfile.jsm");

const recordingState = {
  // The initial state before we've queried the PerfActor
  NOT_YET_KNOWN: "not-yet-known",
  // The profiler is available, we haven't started recording yet.
  AVAILABLE_TO_RECORD: "available-to-record",
  // An async request has been sent to start the profiler.
  REQUEST_TO_START_RECORDING: "request-to-start-recording",
  // An async request has been sent to get the profile and stop the profiler.
  REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER:
    "request-to-get-profile-and-stop-profiler",
  // An async request has been sent to stop the profiler.
  REQUEST_TO_STOP_PROFILER: "request-to-stop-profiler",
  // The profiler notified us that our request to start it actually started it.
  RECORDING: "recording",
  // Some other code with access to the profiler started it.
  OTHER_IS_RECORDING: "other-is-recording",
  // Profiling is not available when in private browsing mode.
  LOCKED_BY_PRIVATE_BROWSING: "locked-by-private-browsing",
};

const UNITS = ["B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"];

/**
 * Linearly interpolate between values.
 * https://en.wikipedia.org/wiki/Linear_interpolation
 *
 * @param {number} frac - Value ranged 0 - 1 to interpolate between the range
 *                        start and range end.
 * @param {number} rangeState - The value to start from.
 * @param {number} rangeEnd - The value to interpolate to.
 * @returns {number}
 */
function lerp(frac, rangeStart, rangeEnd) {
  return (1 - frac) * rangeStart + frac * rangeEnd;
}

/**
 * Make sure a value is clamped between a min and max value.
 *
 * @param {number} val - The value to clamp.
 * @param {number} min - The minimum value.
 * @returns {number}
 */
function clamp(val, min, max) {
  return Math.max(min, Math.min(max, val));
}

/**
 * Formats a file size.
 * @param {number} num - The number (in bytes) to format.
 * @returns {string} e.g. "10 B", "100 MB"
 */
function formatFileSize(num) {
  if (!Number.isFinite(num)) {
    throw new TypeError(`Expected a finite number, got ${typeof num}: ${num}`);
  }

  const neg = num < 0;

  if (neg) {
    num = -num;
  }

  if (num < 1) {
    return (neg ? "-" : "") + num + " B";
  }

  const exponent = Math.min(
    Math.floor(Math.log(num) / Math.log(1000)),
    UNITS.length - 1
  );
  const numStr = Number((num / Math.pow(1000, exponent)).toPrecision(3));
  const unit = UNITS[exponent];

  return (neg ? "-" : "") + numStr + " " + unit;
}

/**
 * Creates numbers that scale exponentially.
 *
 * @param {number} rangeStart
 * @param {number} rangeEnd
 */
function makeExponentialScale(rangeStart, rangeEnd) {
  const startExp = Math.log(rangeStart);
  const endExp = Math.log(rangeEnd);
  const fromFractionToValue = frac =>
    Math.exp((1 - frac) * startExp + frac * endExp);
  const fromValueToFraction = value =>
    (Math.log(value) - startExp) / (endExp - startExp);
  const fromFractionToSingleDigitValue = frac => {
    return +fromFractionToValue(frac).toPrecision(1);
  };
  return {
    // Takes a number ranged 0-1 and returns it within the range.
    fromFractionToValue,
    // Takes a number in the range, and returns a value between 0-1
    fromValueToFraction,
    // Takes a number ranged 0-1 and returns a value in the range, but with
    // a single digit value.
    fromFractionToSingleDigitValue,
  };
}

/**
 * Scale a source range to a destination range, but clamp it within the
 * destination range.
 * @param {number} val - The source range value to map to the destination range,
 * @param {number} sourceRangeStart,
 * @param {number} sourceRangeEnd,
 * @param {number} destRangeStart,
 * @param {number} destRangeEnd
 */
function scaleRangeWithClamping(
  val,
  sourceRangeStart,
  sourceRangeEnd,
  destRangeStart,
  destRangeEnd
) {
  const frac = clamp(
    (val - sourceRangeStart) / (sourceRangeEnd - sourceRangeStart),
    0,
    1
  );
  return lerp(frac, destRangeStart, destRangeEnd);
}

/**
 * Use some heuristics to guess at the overhead of the recording settings.
 * @param {number} interval
 * @param {number} bufferSize
 * @param {array} features - List of the selected features.
 */
function calculateOverhead(interval, bufferSize, features) {
  const overheadFromSampling =
    scaleRangeWithClamping(
      Math.log(interval),
      Math.log(0.05),
      Math.log(1),
      1,
      0
    ) +
    scaleRangeWithClamping(
      Math.log(interval),
      Math.log(1),
      Math.log(100),
      0.1,
      0
    );
  const overheadFromBuffersize = scaleRangeWithClamping(
    Math.log(bufferSize),
    Math.log(10),
    Math.log(1000000),
    0,
    0.1
  );
  const overheadFromStackwalk = features.includes("stackwalk") ? 0.05 : 0;
  const overheadFromJavaScrpt = features.includes("js") ? 0.05 : 0;
  const overheadFromTaskTracer = features.includes("tasktracer") ? 0.05 : 0;
  return clamp(
    overheadFromSampling +
      overheadFromBuffersize +
      overheadFromStackwalk +
      overheadFromJavaScrpt +
      overheadFromTaskTracer,
    0,
    1
  );
}

/**
 * Given an array of absolute paths on the file system, return an array that
 * doesn't contain the common prefix of the paths; in other words, if all paths
 * share a common ancestor directory, cut off the path to that ancestor
 * directory and only leave the path components that differ.
 * This makes some lists look a little nicer. For example, this turns the list
 * ["/Users/foo/code/obj-m-android-opt", "/Users/foo/code/obj-m-android-debug"]
 * into the list ["obj-m-android-opt", "obj-m-android-debug"].
 * @param {array of string} pathArray The array of absolute paths.
 * @returns {array of string} A new array with the described adjustment.
 */
function withCommonPathPrefixRemoved(pathArray) {
  if (pathArray.length === 0) {
    return [];
  }
  const splitPaths = pathArray.map(path => OS.Path.split(path));
  if (!splitPaths.every(sp => sp.absolute)) {
    // We're expecting all paths to be absolute, so this is an unexpected case,
    // return the original array.
    return pathArray;
  }
  const [firstSplitPath, ...otherSplitPaths] = splitPaths;
  if ("winDrive" in firstSplitPath) {
    const winDrive = firstSplitPath.winDrive;
    if (!otherSplitPaths.every(sp => sp.winDrive === winDrive)) {
      return pathArray;
    }
  } else if (otherSplitPaths.some(sp => "winDrive" in sp)) {
    // Inconsistent winDrive property presence, bail out.
    return pathArray;
  }
  // At this point we're either not on Windows or all paths are on the same
  // winDrive. And all paths are absolute.
  // Find the common prefix. Start by assuming the entire path except for the
  // last folder is shared.
  const prefix = firstSplitPath.components.slice(0, -1);
  for (const sp of otherSplitPaths) {
    prefix.length = Math.min(prefix.length, sp.components.length - 1);
    for (let i = 0; i < prefix.length; i++) {
      if (prefix[i] !== sp.components[i]) {
        prefix.length = i;
        break;
      }
    }
  }
  if (prefix.length === 0 || (prefix.length === 1 && prefix[0] === "")) {
    // There is no shared prefix.
    // We treat a prefix of [""] as "no prefix", too: Absolute paths on
    // non-Windows start with a slash, so OS.Path.split(path) always returns an
    // array whose first element is the empty string on those platforms.
    // Stripping off a prefix of [""] from the split paths would simply remove
    // the leading slash from the un-split paths, which is not useful.
    return pathArray;
  }
  return splitPaths.map(sp =>
    OS.Path.join(...sp.components.slice(prefix.length))
  );
}

module.exports = {
  formatFileSize,
  makeExponentialScale,
  scaleRangeWithClamping,
  calculateOverhead,
  recordingState,
  withCommonPathPrefixRemoved,
};
