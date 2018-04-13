/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

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

module.exports = {
  formatFileSize,
  makeExponentialScale,
  scaleRangeWithClamping,
  calculateOverhead,
  recordingState
};
