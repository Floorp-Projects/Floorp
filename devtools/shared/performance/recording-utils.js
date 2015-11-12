/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
loader.lazyRequireGetter(this, "extend",
  "sdk/util/object", true);

/**
 * Utility functions for managing recording models and their internal data,
 * such as filtering profile samples or offsetting timestamps.
 */

function mapRecordingOptions (type, options) {
  if (type === "profiler") {
    return {
      entries: options.bufferSize,
      interval: options.sampleFrequency ? (1000 / (options.sampleFrequency * 1000)) : void 0
    };
  }

  if (type === "memory") {
    return {
      probability: options.allocationsSampleProbability,
      maxLogLength: options.allocationsMaxLogLength
    };
  }

  if (type === "timeline") {
    return {
      withMarkers: true,
      withTicks: options.withTicks,
      withMemory: options.withMemory,
      withFrames: true,
      withGCEvents: true,
      withDocLoadingEvents: false
    };
  }

  return options;
}

/**
 * Takes an options object for `startRecording`, and normalizes
 * it based off of server support. For example, if the user
 * requests to record memory `withMemory = true`, but the server does
 * not support that feature, then the `false` will overwrite user preference
 * in order to define the recording with what is actually available, not
 * what the user initially requested.
 *
 * @param {object} options
 * @param {boolean}
 */
function normalizePerformanceFeatures (options, supportedFeatures) {
  return Object.keys(options).reduce((modifiedOptions, feature) => {
    if (supportedFeatures[feature] !== false) {
      modifiedOptions[feature] = options[feature];
    }
    return modifiedOptions;
  }, Object.create(null));
}

/**
 * Filters all the samples in the provided profiler data to be more recent
 * than the specified start time.
 *
 * @param object profile
 *        The profiler data received from the backend.
 * @param number profilerStartTime
 *        The earliest acceptable sample time (in milliseconds).
 */
function filterSamples(profile, profilerStartTime) {
  let firstThread = profile.threads[0];
  const TIME_SLOT = firstThread.samples.schema.time;
  firstThread.samples.data = firstThread.samples.data.filter(e => {
    return e[TIME_SLOT] >= profilerStartTime;
  });
}

/**
 * Offsets all the samples in the provided profiler data by the specified time.
 *
 * @param object profile
 *        The profiler data received from the backend.
 * @param number timeOffset
 *        The amount of time to offset by (in milliseconds).
 */
function offsetSampleTimes(profile, timeOffset) {
  let firstThread = profile.threads[0];
  const TIME_SLOT = firstThread.samples.schema.time;
  let samplesData = firstThread.samples.data;
  for (let i = 0; i < samplesData.length; i++) {
    samplesData[i][TIME_SLOT] -= timeOffset;
  }
}

/**
 * Offsets all the markers in the provided timeline data by the specified time.
 *
 * @param array markers
 *        The markers array received from the backend.
 * @param number timeOffset
 *        The amount of time to offset by (in milliseconds).
 */
function offsetMarkerTimes(markers, timeOffset) {
  for (let marker of markers) {
    marker.start -= timeOffset;
    marker.end -= timeOffset;
  }
}

/**
 * Offsets and scales all the timestamps in the provided array by the
 * specified time and scale factor.
 *
 * @param array array
 *        A list of timestamps received from the backend.
 * @param number timeOffset
 *        The amount of time to offset by (in milliseconds).
 * @param number timeScale
 *        The factor to scale by, after offsetting.
 */
function offsetAndScaleTimestamps(timestamps, timeOffset, timeScale) {
  for (let i = 0, len = timestamps.length; i < len; i++) {
    timestamps[i] -= timeOffset;
    if (timeScale) {
      timestamps[i] /= timeScale;
    }
  }
}

/**
 * Push all elements of src array into dest array. Marker data will come in small chunks
 * and add up over time, whereas allocation arrays can be > 500000 elements (and
 * Function.prototype.apply throws if applying more than 500000 elements, which
 * is what spawned this separate function), so iterate one element at a time.
 * @see bug 1166823
 * @see http://jsperf.com/concat-large-arrays
 * @see http://jsperf.com/concat-large-arrays/2
 *
 * @param {Array} dest
 * @param {Array} src
 */
function pushAll (dest, src) {
  let length = src.length;
  for (let i = 0; i < length; i++) {
    dest.push(src[i]);
  }
}

/**
 * Cache used in `RecordingUtils.getProfileThreadFromAllocations`.
 */
var gProfileThreadFromAllocationCache = new WeakMap();

/**
 * Converts allocation data from the memory actor to something that follows
 * the same structure as the samples data received from the profiler.
 *
 * @see MemoryActor.prototype.getAllocations for more information.
 *
 * @param object allocations
 *        A list of { sites, timestamps, frames, sizes } arrays.
 * @return object
 *         The "profile" describing the allocations log.
 */
function getProfileThreadFromAllocations(allocations) {
  let cached = gProfileThreadFromAllocationCache.get(allocations);
  if (cached) {
    return cached;
  }

  let { sites, timestamps, frames, sizes } = allocations;
  let uniqueStrings = new UniqueStrings();

  // Convert allocation frames to the the stack and frame tables expected by
  // the profiler format.
  //
  // Since the allocations log is already presented as a tree, we would be
  // wasting time if we jumped through the same hoops as deflateProfile below
  // and instead use the existing structure of the allocations log to build up
  // the profile JSON.
  //
  // The allocations.frames array corresponds roughly to the profile stack
  // table: a trie of all stacks. We could work harder to further deduplicate
  // each individual frame as the profiler does, but it is not necessary for
  // correctness.
  let stackTable = new Array(frames.length);
  let frameTable = new Array(frames.length);

  // Array used to concat the location.
  let locationConcatArray = new Array(5);

  for (let i = 0; i < frames.length; i++) {
    let frame = frames[i];
    if (!frame) {
      stackTable[i] = frameTable[i] = null;
      continue;
    }

    let prefix = frame.parent;

    // Schema:
    //   [prefix, frame]
    stackTable[i] = [frames[prefix] ? prefix : null, i];

    // Schema:
    //   [location]
    //
    // The only field a frame will have in an allocations profile is location.
    //
    // If frame.functionDisplayName is present, the format is
    //   "functionDisplayName (source:line:column)"
    // Otherwise, it is
    //   "source:line:column"
    //
    // A static array is used to join to save memory on intermediate strings.
    locationConcatArray[0] = frame.source;
    locationConcatArray[1] = ":";
    locationConcatArray[2] = String(frame.line);
    locationConcatArray[3] = ":";
    locationConcatArray[4] = String(frame.column);
    locationConcatArray[5] = "";

    let location = locationConcatArray.join("");
    let funcName = frame.functionDisplayName;

    if (funcName) {
      locationConcatArray[0] = funcName;
      locationConcatArray[1] = " (";
      locationConcatArray[2] = location;
      locationConcatArray[3] = ")";
      locationConcatArray[4] = "";
      locationConcatArray[5] = "";
      location = locationConcatArray.join("");
    }

    frameTable[i] = [uniqueStrings.getOrAddStringIndex(location)];
  }

  let samples = new Array(sites.length);
  let writePos = 0;
  for (let i = 0; i < sites.length; i++) {
    // Schema:
    //   [stack, time, size]
    //
    // Originally, sites[i] indexes into the frames array. Note that in the
    // loop above, stackTable[sites[i]] and frames[sites[i]] index the same
    // information.
    let stackIndex = sites[i];
    if (frames[stackIndex]) {
      samples[writePos++] = [stackIndex, timestamps[i], sizes[i]];
    }
  }
  samples.length = writePos;

  let thread = {
    name: "allocations",
    samples: allocationsWithSchema(samples),
    stackTable: stackTableWithSchema(stackTable),
    frameTable: frameTableWithSchema(frameTable),
    stringTable: uniqueStrings.stringTable
  };

  gProfileThreadFromAllocationCache.set(allocations, thread);
  return thread;
}

function allocationsWithSchema (data) {
  let slot = 0;
  return {
    schema: {
      stack: slot++,
      time: slot++,
      size: slot++,
    },
    data: data
  };
}

/**
 * Deduplicates a profile by deduplicating stacks, frames, and strings.
 *
 * This is used to adapt version 2 profiles from the backend to version 3, for
 * use with older Geckos (like B2G).
 *
 * Note that the schemas used by this must be kept in sync with schemas used
 * by the C++ UniqueStacks class in tools/profiler/ProfileEntry.cpp.
 *
 * @param object profile
 *               A profile with version 2.
 */
function deflateProfile(profile) {
  profile.threads = profile.threads.map((thread) => {
    let uniqueStacks = new UniqueStacks();
    return deflateThread(thread, uniqueStacks);
  });

  profile.meta.version = 3;
  return profile;
};

/**
 * Given an array of frame objects, deduplicates each frame as well as all
 * prefixes in the stack. Returns the index of the deduplicated stack.
 *
 * @param object frames
 *               Array of frame objects.
 * @param UniqueStacks uniqueStacks
 * @return number index
 */
function deflateStack(frames, uniqueStacks) {
  // Deduplicate every prefix in the stack by keeping track of the current
  // prefix hash.
  let prefixIndex = null;
  for (let i = 0; i < frames.length; i++) {
    let frameIndex = uniqueStacks.getOrAddFrameIndex(frames[i]);
    prefixIndex = uniqueStacks.getOrAddStackIndex(prefixIndex, frameIndex);
  }
  return prefixIndex;
}

/**
 * Given an array of sample objects, deduplicate each sample's stack and
 * convert the samples to a table with a schema. Returns the deflated samples.
 *
 * @param object samples
 *               Array of samples
 * @param UniqueStacks uniqueStacks
 * @return object
 */
function deflateSamples(samples, uniqueStacks) {
  // Schema:
  //   [stack, time, responsiveness, rss, uss, frameNumber, power]

  let deflatedSamples = new Array(samples.length);
  for (let i = 0; i < samples.length; i++) {
    let sample = samples[i];
    deflatedSamples[i] = [
      deflateStack(sample.frames, uniqueStacks),
      sample.time,
      sample.responsiveness,
      sample.rss,
      sample.uss,
      sample.frameNumber,
      sample.power
    ];
  }

  return samplesWithSchema(deflatedSamples);
}

/**
 * Given an array of marker objects, convert the markers to a table with a
 * schema. Returns the deflated markers.
 *
 * If a marker contains a backtrace as its payload, the backtrace stack is
 * deduplicated in the context of the profile it's in.
 *
 * @param object markers
 *               Array of markers
 * @param UniqueStacks uniqueStacks
 * @return object
 */
function deflateMarkers(markers, uniqueStacks) {
  // Schema:
  //   [name, time, data]

  let deflatedMarkers = new Array(markers.length);
  for (let i = 0; i < markers.length; i++) {
    let marker = markers[i];
    if (marker.data && marker.data.type === "tracing" && marker.data.stack) {
      marker.data.stack = deflateThread(marker.data.stack, uniqueStacks);
    }

    deflatedMarkers[i] = [
      uniqueStacks.getOrAddStringIndex(marker.name),
      marker.time,
      marker.data
    ];
  }

  let slot = 0;
  return {
    schema: {
      name: slot++,
      time: slot++,
      data: slot++
    },
    data: deflatedMarkers
  };
}

/**
 * Deflate a thread.
 *
 * @param object thread
 *               The profile thread.
 * @param UniqueStacks uniqueStacks
 * @return object
 */
function deflateThread(thread, uniqueStacks) {
  // Some extra threads in a profile come stringified as a full profile (so
  // it has nested threads itself) so the top level "thread" does not have markers
  // or samples. We don't use this anyway so just make this safe to deflate.
  // can be a string rather than an object on import. Bug 1173695
  if (typeof thread === "string") {
    thread = JSON.parse(thread);
  }
  if (!thread.samples) {
    thread.samples = [];
  }
  if (!thread.markers) {
    thread.markers = [];
  }

  return {
    name: thread.name,
    tid: thread.tid,
    samples: deflateSamples(thread.samples, uniqueStacks),
    markers: deflateMarkers(thread.markers, uniqueStacks),
    stackTable: uniqueStacks.getStackTableWithSchema(),
    frameTable: uniqueStacks.getFrameTableWithSchema(),
    stringTable: uniqueStacks.getStringTable()
  };
}

function stackTableWithSchema(data) {
  let slot = 0;
  return {
    schema: {
      prefix: slot++,
      frame: slot++
    },
    data: data
  };
}

function frameTableWithSchema(data) {
  let slot = 0;
  return {
    schema: {
      location: slot++,
      implementation: slot++,
      optimizations: slot++,
      line: slot++,
      category: slot++
    },
    data: data
  };
}

function samplesWithSchema(data) {
  let slot = 0;
  return {
    schema: {
      stack: slot++,
      time: slot++,
      responsiveness: slot++,
      rss: slot++,
      uss: slot++,
      frameNumber: slot++,
      power: slot++
    },
    data: data
  };
}

/**
 * A helper class to deduplicate strings.
 */
function UniqueStrings() {
  this.stringTable = [];
  this._stringHash = Object.create(null);
}

UniqueStrings.prototype.getOrAddStringIndex = function(s) {
  if (!s) {
    return null;
  }

  let stringHash = this._stringHash;
  let stringTable = this.stringTable;
  let index = stringHash[s];
  if (index !== undefined) {
    return index;
  }

  index = stringTable.length;
  stringHash[s] = index;
  stringTable.push(s);
  return index;
};

/**
 * A helper class to deduplicate old-version profiles.
 *
 * The main functionality provided is deduplicating frames and stacks.
 *
 * For example, given 2 stacks
 *   [A, B, C]
 * and
 *   [A, B, D]
 *
 * There are 4 unique frames: A, B, C, and D.
 * There are 4 unique prefixes: [A], [A, B], [A, B, C], [A, B, D]
 *
 * For the example, the output of using UniqueStacks is:
 *
 * Frame table:
 *   [A, B, C, D]
 *
 * That is, A has id 0, B has id 1, etc.
 *
 * Since stack prefixes are themselves deduplicated (shared), stacks are
 * represented as a tree, or more concretely, a pair of ids, the prefix and
 * the leaf.
 *
 * Stack table:
 *   [
 *     [null, 0],
 *     [0,    1],
 *     [1,    2],
 *     [1,    3]
 *   ]
 *
 * That is, [A] has id 0 and value [null, 0]. This means it has no prefix, and
 * has the leaf frame 0, which resolves to A in the frame table.
 *
 * [A, B] has id 1 and value [0, 1]. This means it has prefix 0, which is [A],
 * and leaf 1, thus [A, B].
 *
 * [A, B, C] has id 2 and value [1, 2]. This means it has prefix 1, which in
 * turn is [A, B], and leaf 2, thus [A, B, C].
 *
 * [A, B, D] has id 3 and value [1, 3]. Note how it shares the prefix 1 with
 * [A, B, C].
 */
function UniqueStacks() {
  this._frameTable = [];
  this._stackTable = [];
  this._frameHash = Object.create(null);
  this._stackHash = Object.create(null);
  this._uniqueStrings = new UniqueStrings();
}

UniqueStacks.prototype.getStackTableWithSchema = function() {
  return stackTableWithSchema(this._stackTable);
};

UniqueStacks.prototype.getFrameTableWithSchema = function() {
  return frameTableWithSchema(this._frameTable);
};

UniqueStacks.prototype.getStringTable = function() {
  return this._uniqueStrings.stringTable;
};

UniqueStacks.prototype.getOrAddFrameIndex = function(frame) {
  // Schema:
  //   [location, implementation, optimizations, line, category]

  let frameHash = this._frameHash;
  let frameTable = this._frameTable;

  let locationIndex = this.getOrAddStringIndex(frame.location);
  let implementationIndex = this.getOrAddStringIndex(frame.implementation);

  // Super dumb.
  let hash = `${locationIndex} ${implementationIndex || ""} ${frame.line || ""} ${frame.category || ""}`;

  let index = frameHash[hash];
  if (index !== undefined) {
    return index;
  }

  index = frameTable.length;
  frameHash[hash] = index;
  frameTable.push([
    this.getOrAddStringIndex(frame.location),
    this.getOrAddStringIndex(frame.implementation),
    // Don't bother with JIT optimization info for deflating old profile data
    // format to the new format.
    null,
    frame.line,
    frame.category
  ]);
  return index;
};

UniqueStacks.prototype.getOrAddStackIndex = function(prefixIndex, frameIndex) {
  // Schema:
  //   [prefix, frame]

  let stackHash = this._stackHash;
  let stackTable = this._stackTable;

  // Also super dumb.
  let hash = prefixIndex + " " + frameIndex;

  let index = stackHash[hash];
  if (index !== undefined) {
    return index;
  }

  index = stackTable.length;
  stackHash[hash] = index;
  stackTable.push([prefixIndex, frameIndex]);
  return index;
};

UniqueStacks.prototype.getOrAddStringIndex = function(s) {
  return this._uniqueStrings.getOrAddStringIndex(s);
};

exports.pushAll = pushAll;
exports.mapRecordingOptions = mapRecordingOptions;
exports.normalizePerformanceFeatures = normalizePerformanceFeatures;
exports.filterSamples = filterSamples;
exports.offsetSampleTimes = offsetSampleTimes;
exports.offsetMarkerTimes = offsetMarkerTimes;
exports.offsetAndScaleTimestamps = offsetAndScaleTimestamps;
exports.getProfileThreadFromAllocations = getProfileThreadFromAllocations;
exports.deflateProfile = deflateProfile;
exports.deflateThread = deflateThread;
exports.UniqueStrings = UniqueStrings;
exports.UniqueStacks = UniqueStacks;
