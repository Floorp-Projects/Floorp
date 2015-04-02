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
 * When the ThreadNode for the profile iterates over the samples' frames, a JITOptimization
 * model is attached to each frame node, with each sample of the frame, usually with each
 * sample containing different optimization information for the same frame (one sample may
 * pick up optimization X on line Y in the frame, with the next sample containing optimization Z
 * on line W in the same frame, as each frame is only function.
 *
 * Each RawOptimizationSite can be sampled multiple times, which multiple calls to
 * JITOptimizations#addOptimizationSite handles. An OptimizationSite contains
 * a record of how many times the RawOptimizationSite was sampled, as well as the unique id
 * based off of the original profiler array, and the RawOptimizationSite itself as a reference.
 * @see browser/devtools/shared/profiler/tree-model.js
 *
 *
 * @struct RawOptimizationSite
 * A structure describing a location in a script that was attempted to be optimized.
 * Contains all the IonTypes observed, and the sequence of OptimizationAttempts that
 * were attempted, and the line and column in the script. This is retrieved from the
 * profiler after a recording, and our base data structure. Should always be referenced,
 * and unmodified.
 *
 * @type {Array<IonType>} types
 * @type {Array<OptimizationAttempt>} attempts
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
 * @type {?Array<ObservedType>} types
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

const OptimizationSite = exports.OptimizationSite = function (optimizations, optsIndex) {
  this.id = optsIndex;
  this.data = optimizations[optsIndex];
  this.samples = 0;
};

/**
 * Returns a boolean indicating if the passed in OptimizationSite
 * has a "good" outcome at the end of its attempted strategies.
 *
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
 * @param {Array<RawOptimizationSite>} optimizations
 *        Array of RawOptimizationSites from the profiler. Do not modify this!
 */

const JITOptimizations = exports.JITOptimizations = function (optimizations) {
  this._opts = optimizations;
  // Hash of OptimizationSites observed for this frame.
  this._optSites = {};
};

/**
 * Called when a sample detects an optimization on this frame. Takes an `optsIndex`,
 * referring to an optimization in the stored `this._opts` array. Creates a histogram
 * of optimization site data by creating or incrementing an OptimizationSite
 * for each observed optimization.
 *
 * @param {Number} optsIndex
 */

JITOptimizations.prototype.addOptimizationSite = function (optsIndex) {
  let op = this._optSites[optsIndex] || (this._optSites[optsIndex] = new OptimizationSite(this._opts, optsIndex));
  op.samples++;
};

/**
 * Returns an array of OptimizationSites, sorted from most to least times sampled.
 *
 * @return {Array<OptimizationSite>}
 */

JITOptimizations.prototype.getOptimizationSites = function () {
  let opts = [];
  for (let opt of Object.keys(this._optSites)) {
    opts.push(this._optSites[opt]);
  }
  return opts.sort((a, b) => b.samples - a.samples);
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
