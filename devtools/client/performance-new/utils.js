/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
/**
 * @typedef {import("./@types/perf").NumberScaler} NumberScaler
 * @typedef {import("./@types/perf").ScaleFunctions} ScaleFunctions
 * @typedef {import("./@types/perf").FeatureDescription} FeatureDescription
 */
"use strict";

// @ts-ignore
const { OS } = require("resource://gre/modules/osfile.jsm");

const UNITS = ["B", "kiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"];

/**
 * Linearly interpolate between values.
 * https://en.wikipedia.org/wiki/Linear_interpolation
 *
 * @param {number} frac - Value ranged 0 - 1 to interpolate between the range start and range end.
 * @param {number} rangeStart - The value to start from.
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
 * @param {number} max - The max value.
 * @returns {number}
 */
function clamp(val, min, max) {
  return Math.max(min, Math.min(max, val));
}

/**
 * Formats a file size.
 * @param {number} num - The number (in bytes) to format.
 * @returns {string} e.g. "10 B", "100 MiB"
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
    Math.floor(Math.log2(num) / Math.log2(1024)),
    UNITS.length - 1
  );
  const numStr = Number((num / Math.pow(1024, exponent)).toPrecision(3));
  const unit = UNITS[exponent];

  return (neg ? "-" : "") + numStr + " " + unit;
}

/**
 * Creates numbers that scale exponentially.
 *
 * @param {number} rangeStart
 * @param {number} rangeEnd
 *
 * @returns {ScaleFunctions}
 */
function makeExponentialScale(rangeStart, rangeEnd) {
  const startExp = Math.log(rangeStart);
  const endExp = Math.log(rangeEnd);

  /** @type {NumberScaler} */
  const fromFractionToValue = frac =>
    Math.exp((1 - frac) * startExp + frac * endExp);

  /** @type {NumberScaler} */
  const fromValueToFraction = value =>
    (Math.log(value) - startExp) / (endExp - startExp);

  /** @type {NumberScaler} */
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
 * Creates numbers that scale exponentially as powers of 2.
 *
 * @param {number} rangeStart
 * @param {number} rangeEnd
 *
 * @returns {ScaleFunctions}
 */
function makePowerOf2Scale(rangeStart, rangeEnd) {
  const startExp = Math.log2(rangeStart);
  const endExp = Math.log2(rangeEnd);

  /** @type {NumberScaler} */
  const fromFractionToValue = frac =>
    Math.pow(2, Math.round((1 - frac) * startExp + frac * endExp));

  /** @type {NumberScaler} */
  const fromValueToFraction = value =>
    (Math.log2(value) - startExp) / (endExp - startExp);

  /** @type {NumberScaler} */
  const fromFractionToSingleDigitValue = frac => {
    // fromFractionToValue returns an exact power of 2, we don't want to change
    // its precision. Note that formatFileSize will display it in a nice binary
    // unit with up to 3 digits.
    return fromFractionToValue(frac);
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
 *
 * TODO - Bug 1597383. The UI for this has been removed, but it needs to be reworked
 * for new overhead calculations. Keep it for now in tree.
 *
 * @param {number} interval
 * @param {number} bufferSize
 * @param {string[]} features - List of the selected features.
 */
function calculateOverhead(interval, bufferSize, features) {
  // NOT "nostacksampling" (double negative) means periodic sampling is on.
  const periodicSampling = !features.includes("nostacksampling");
  const overheadFromSampling = periodicSampling
    ? scaleRangeWithClamping(
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
      )
    : 0;
  const overheadFromBuffersize = scaleRangeWithClamping(
    Math.log(bufferSize),
    Math.log(10),
    Math.log(1000000),
    0,
    0.1
  );
  const overheadFromStackwalk =
    features.includes("stackwalk") && periodicSampling ? 0.05 : 0;
  const overheadFromJavaScript =
    features.includes("js") && periodicSampling ? 0.05 : 0;
  const overheadFromTaskTracer = features.includes("tasktracer") ? 0.05 : 0;
  const overheadFromJSTracer = features.includes("jstracer") ? 0.05 : 0;
  const overheadFromJSAllocations = features.includes("jsallocations")
    ? 0.05
    : 0;
  const overheadFromNativeAllocations = features.includes("nativeallocations")
    ? 0.5
    : 0;

  return clamp(
    overheadFromSampling +
      overheadFromBuffersize +
      overheadFromStackwalk +
      overheadFromJavaScript +
      overheadFromTaskTracer +
      overheadFromJSTracer +
      overheadFromJSAllocations +
      overheadFromNativeAllocations,
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
 *
 * @param {string[]} pathArray The array of absolute paths.
 * @returns {string[]} A new array with the described adjustment.
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

class UnhandledCaseError extends Error {
  /**
   * @param {never} value - Check that
   * @param {string} typeName - A friendly type name.
   */
  constructor(value, typeName) {
    super(`There was an unhandled case for "${typeName}": ${value}`);
    this.name = "UnhandledCaseError";
  }
}

/**
 * @type {FeatureDescription[]}
 */
const featureDescriptions = [
  {
    name: "Native Stacks",
    value: "stackwalk",
    title:
      "Record native stacks (C++ and Rust). This is not available on all platforms.",
    recommended: true,
    disabledReason: "Native stack walking is not supported on this platform.",
  },
  {
    name: "JavaScript",
    value: "js",
    title:
      "Record JavaScript stack information, and interleave it with native stacks.",
    recommended: true,
  },
  {
    name: "Responsiveness",
    value: "responsiveness",
    title: "Collect thread responsiveness information.",
    recommended: true,
  },
  {
    name: "Java",
    value: "java",
    title: "Profile Java code",
    disabledReason: "This feature is only available on Android.",
  },
  {
    name: "Native Leaf Stack",
    value: "leaf",
    title:
      "Record the native memory address of the leaf-most stack. This could be " +
      "useful on platforms that do not support stack walking.",
  },
  {
    name: "No Periodic Sampling",
    value: "nostacksampling",
    title: "Disable interval-based stack sampling",
  },
  {
    name: "Main Thread IO",
    value: "mainthreadio",
    title: "Record main thread I/O markers.",
  },
  {
    name: "Privacy",
    value: "privacy",
    title: "Remove some potentially user-identifiable information.",
  },
  {
    name: "Sequential Styling",
    value: "seqstyle",
    title: "Disable parallel traversal in styling.",
  },
  {
    name: "JIT Optimizations",
    value: "trackopts",
    title: "Track JIT optimizations in the JS engine.",
  },
  {
    name: "TaskTracer",
    value: "tasktracer",
    title: "Enable TaskTracer (Experimental.)",
    disabledReason:
      "TaskTracer requires a custom build with the environment variable MOZ_TASK_TRACER set.",
  },
  {
    name: "Screenshots",
    value: "screenshots",
    title: "Record screenshots of all browser windows.",
  },
  {
    name: "JSTracer",
    value: "jstracer",
    title: "Trace JS engine (Experimental.)",
    disabledReason:
      "JS Tracer is currently disabled due to crashes. See Bug 1565788.",
  },
  {
    name: "Preference Read",
    value: "preferencereads",
    title: "Track Preference Reads",
  },
  {
    name: "IPC Messages",
    value: "ipcmessages",
    title: "Track IPC messages.",
  },
  {
    name: "JS Allocations",
    value: "jsallocations",
    title: "Track JavaScript allocations (Experimental.)",
  },
  {
    name: "Native Allocations",
    value: "nativeallocations",
    title: "Track native allocations (Experimental.)",
  },
];

module.exports = {
  formatFileSize,
  makeExponentialScale,
  makePowerOf2Scale,
  scaleRangeWithClamping,
  calculateOverhead,
  withCommonPathPrefixRemoved,
  UnhandledCaseError,
  featureDescriptions,
};
