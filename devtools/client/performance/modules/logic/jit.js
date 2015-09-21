/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// An outcome of an OptimizationAttempt that is considered successful.
const SUCCESSFUL_OUTCOMES = [
  "GenericSuccess", "Inlined", "DOM", "Monomorphic", "Polymorphic"
];

/**
 * Model representing JIT optimization sites from the profiler
 * for a frame (represented by a FrameNode). Requires optimization data from
 * a profile, which is an array of RawOptimizationSites.
 *
 * When the ThreadNode for the profile iterates over the samples' frames, each
 * frame's optimizations are accumulated in their respective FrameNodes. Each
 * FrameNode may contain many different optimization sites. One sample may
 * pick up optimization X on line Y in the frame, with the next sample
 * containing optimization Z on line W in the same frame, as each frame is
 * only function.
 *
 * An OptimizationSite contains a record of how many times the
 * RawOptimizationSite was sampled, as well as the unique id based off of the
 * original profiler array, and the RawOptimizationSite itself as a reference.
 * @see devtools/client/performance/modules/logic/tree-model.js
 *
 * @struct RawOptimizationSite
 * A structure describing a location in a script that was attempted to be optimized.
 * Contains all the IonTypes observed, and the sequence of OptimizationAttempts that
 * were attempted, and the line and column in the script. This is retrieved from the
 * profiler after a recording, and our base data structure. Should always be referenced,
 * and unmodified.
 *
 * Note that propertyName is an index into a string table, which needs to be
 * provided in order for the raw optimization site to be inflated.
 *
 * @type {Array<IonType>} types
 * @type {Array<OptimizationAttempt>} attempts
 * @type {?number} propertyName
 * @type {number} line
 * @type {number} column
 *
 *
 * @struct IonType
 * IonMonkey attempts to classify each value in an optimization site by some type.
 * Based off of the observed types for a value (like a variable that could be a
 * string or an instance of an object), it determines what kind of type it should be classified
 * as. Each IonType here contains an array of all ObservedTypes under `types`,
 * the Ion type that IonMonkey decided this value should be (Int32, Object, etc.) as `mirType`,
 * and the component of this optimization type that this value refers to -- like
 * a "getter" optimization, `a[b]`, has site `a` (the "Receiver") and `b` (the "Index").
 *
 * Generally the more ObservedTypes, the more deoptimized this OptimizationSite is.
 * There could be no ObservedTypes, in which case `types` is undefined.
 *
 * @type {?Array<ObservedType>} typeset
 * @type {string} site
 * @type {string} mirType
 *
 *
 * @struct ObservedType
 * When IonMonkey attempts to determine what type a value is, it checks on each sample.
 * The ObservedType can be thought of in more of JavaScripty-terms, rather than C++.
 * The `keyedBy` property is a high level description of the type, like "primitive",
 * "constructor", "function", "singleton", "alloc-site" (that one is a bit more weird).
 * If the `keyedBy` type is a function or constructor, the ObservedType should have a
 * `name` property, referring to the function or constructor name from the JS source.
 * If IonMonkey can determine the origin of this type (like where the constructor is defined),
 * the ObservedType will also have `location` and `line` properties, but `location` can sometimes
 * be non-URL strings like "self-hosted" or a memory location like "102ca7880", or no location
 * at all, and maybe `line` is 0 or undefined.
 *
 * @type {string} keyedBy
 * @type {?string} name
 * @type {?string} location
 * @type {?string} line
 *
 *
 * @struct OptimizationAttempt
 * Each RawOptimizationSite contains an array of OptimizationAttempts. Generally, IonMonkey
 * goes through a series of strategies for each kind of optimization, starting from most-niche
 * and optimized, to the less-optimized, but more general strategies -- for example, a getter
 * opt may first try to optimize for the scenario of a getter on an `arguments` object --
 * that will fail most of the time, as most objects are not arguments objects, but it will attempt
 * several strategies in order until it finds a strategy that works, or fails. Even in the best
 * scenarios, some attempts will fail (like the arguments getter example), which is OK,
 * as long as some attempt succeeds (with the earlier attempts preferred, as those are more optimized).
 * In an OptimizationAttempt structure, we store just the `strategy` name and `outcome` name,
 * both from enums in js/public/TrackedOptimizationInfo.h as TRACKED_STRATEGY_LIST and
 * TRACKED_OUTCOME_LIST, respectively. An array of successful outcome strings are above
 * in SUCCESSFUL_OUTCOMES.
 *
 * @see js/public/TrackedOptimizationInfo.h
 *
 * @type {string} strategy
 * @type {string} outcome
 */


/*
 * A wrapper around RawOptimizationSite to record sample count and ID (referring to the index
 * of where this is in the initially seeded optimizations data), so we don't mutate
 * the original data from the profiler. Provides methods to access the underlying optimization
 * data easily, so understanding the semantics of JIT data isn't necessary.
 *
 * @constructor
 *
 * @param {Array<RawOptimizationSite>} optimizations
 * @param {number} optsIndex
 *
 * @type {RawOptimizationSite} data
 * @type {number} samples
 * @type {number} id
 */

const OptimizationSite = function (id, opts) {
  this.id = id;
  this.data = opts;
  this.samples = 1;
};

/**
 * Returns a boolean indicating if the passed in OptimizationSite
 * has a "good" outcome at the end of its attempted strategies.
 *
 * @param {Array<string>} stringTable
 * @return {boolean}
 */

OptimizationSite.prototype.hasSuccessfulOutcome = function () {
  let attempts = this.getAttempts();
  let lastOutcome = attempts[attempts.length - 1].outcome;
  return OptimizationSite.isSuccessfulOutcome(lastOutcome);
};

/**
 * Returns the last attempted OptimizationAttempt for this OptimizationSite.
 *
 * @return {Array<OptimizationAttempt>}
 */

OptimizationSite.prototype.getAttempts = function () {
  return this.data.attempts;
};

/**
 * Returns all IonTypes in this OptimizationSite.
 *
 * @return {Array<IonType>}
 */

OptimizationSite.prototype.getIonTypes = function () {
  return this.data.types;
};


/**
 * Constructor for JITOptimizations. A collection of OptimizationSites for a frame.
 *
 * @constructor
 * @param {Array<RawOptimizationSite>} rawSites
 *                                     Array of raw optimization sites.
 * @param {Array<string>} stringTable
 *                        Array of strings from the profiler used to inflate
 *                        JIT optimizations. Do not modify this!
 */

const JITOptimizations = function (rawSites, stringTable) {
  // Build a histogram of optimization sites.
  let sites = [];

  for (let rawSite of rawSites) {
    let existingSite = sites.find((site) => site.data === rawSite);
    if (existingSite) {
      existingSite.samples++;
    } else {
      sites.push(new OptimizationSite(sites.length, rawSite));
    }
  }

  // Inflate the optimization information.
  for (let site of sites) {
    let data = site.data;
    let STRATEGY_SLOT = data.attempts.schema.strategy;
    let OUTCOME_SLOT = data.attempts.schema.outcome;

    site.data = {
      attempts: data.attempts.data.map((a) => {
        return {
          strategy: stringTable[a[STRATEGY_SLOT]],
          outcome: stringTable[a[OUTCOME_SLOT]]
        }
      }),

      types: data.types.map((t) => {
        return {
          typeset: maybeTypeset(t.typeset, stringTable),
          site: stringTable[t.site],
          mirType: stringTable[t.mirType]
        };
      }),

      propertyName: maybeString(stringTable, data.propertyName),
      line: data.line,
      column: data.column
    };
  }

  this.optimizationSites = sites.sort((a, b) => b.samples - a.samples);
};

/**
 * Make JITOptimizations iterable.
 */
JITOptimizations.prototype = {
  [Symbol.iterator]: function *() {
    yield* this.optimizationSites;
  },

  get length() {
    return this.optimizationSites.length;
  }
};

/**
 * Takes an "outcome" string from an OptimizationAttempt and returns
 * a boolean indicating whether or not its a successful outcome.
 *
 * @return {boolean}
 */

OptimizationSite.isSuccessfulOutcome = JITOptimizations.isSuccessfulOutcome = function (outcome) {
  return !!~SUCCESSFUL_OUTCOMES.indexOf(outcome);
};

function maybeString(stringTable, index) {
  return index ? stringTable[index] : undefined;
}

function maybeTypeset(typeset, stringTable) {
  if (!typeset) {
    return undefined;
  }
  return typeset.map((ty) => {
    return {
      keyedBy: maybeString(stringTable, ty.keyedBy),
      name: maybeString(stringTable, ty.name),
      location: maybeString(stringTable, ty.location),
      line: ty.line
    };
  });
}

// Map of optimization implementation names to an enum.
const IMPLEMENTATION_MAP = {
  "interpreter": 0,
  "baseline": 1,
  "ion": 2
};
const IMPLEMENTATION_NAMES = Object.keys(IMPLEMENTATION_MAP);

/**
 * Takes data from a FrameNode and computes rendering positions for
 * a stacked mountain graph, to visualize JIT optimization tiers over time.
 *
 * @param {FrameNode} frameNode
 *                    The FrameNode who's optimizations we're iterating.
 * @param {Array<number>} sampleTimes
 *                        An array of every sample time within the range we're counting.
 *                        From a ThreadNode's `sampleTimes` property.
 * @param {number} bucketSize
 *                 Size of each bucket in milliseconds.
 *                 `duration / resolution = bucketSize` in OptimizationsGraph.
 * @return {?Array<object>}
 */
function createTierGraphDataFromFrameNode (frameNode, sampleTimes, bucketSize) {
  let tierData = frameNode.getTierData();
  let stringTable = frameNode._stringTable;
  let output = [];
  let implEnum;

  let tierDataIndex = 0;
  let nextOptSample = tierData[tierDataIndex];

  // Bucket data
  let samplesInCurrentBucket = 0;
  let currentBucketStartTime = sampleTimes[0];
  let bucket = [];

  // Store previous data point so we can have straight vertical lines
  let previousValues;

  // Iterate one after the samples, so we can finalize the last bucket
  for (let i = 0; i <= sampleTimes.length; i++) {
    let sampleTime = sampleTimes[i];

    // If this sample is in the next bucket, or we're done
    // checking sampleTimes and on the last iteration, finalize previous bucket
    if (sampleTime >= (currentBucketStartTime + bucketSize) ||
        i >= sampleTimes.length) {

      let dataPoint = {};
      dataPoint.values = [];
      dataPoint.delta = currentBucketStartTime;

      // Map the opt site counts as a normalized percentage (0-1)
      // of its count in context of total samples this bucket
      for (let j = 0; j < IMPLEMENTATION_NAMES.length; j++) {
        dataPoint.values[j] = (bucket[j] || 0) / (samplesInCurrentBucket || 1);
      }

      // Push the values from the previous bucket to the same time
      // as the current bucket so we get a straight vertical line.
      if (previousValues) {
        let data = Object.create(null);
        data.values = previousValues;
        data.delta = currentBucketStartTime;
        output.push(data);
      }

      output.push(dataPoint);

      // Set the new start time of this bucket and reset its count
      currentBucketStartTime += bucketSize;
      samplesInCurrentBucket = 0;
      previousValues = dataPoint.values;
      bucket = [];
    }

    // If this sample observed an optimization in this frame, record it
    if (nextOptSample && nextOptSample.time === sampleTime) {
      // If no implementation defined, it was the "interpreter".
      implEnum = IMPLEMENTATION_MAP[stringTable[nextOptSample.implementation] || "interpreter"];
      bucket[implEnum] = (bucket[implEnum] || 0) + 1;
      nextOptSample = tierData[++tierDataIndex];
    }

    samplesInCurrentBucket++;
  }

  return output;
}

exports.createTierGraphDataFromFrameNode = createTierGraphDataFromFrameNode;
exports.OptimizationSite = OptimizationSite;
exports.JITOptimizations = JITOptimizations;
