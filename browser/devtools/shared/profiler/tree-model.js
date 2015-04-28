/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

loader.lazyRequireGetter(this, "L10N",
  "devtools/shared/profiler/global", true);
loader.lazyRequireGetter(this, "CATEGORY_MAPPINGS",
  "devtools/shared/profiler/global", true);
loader.lazyRequireGetter(this, "CATEGORIES",
  "devtools/shared/profiler/global", true);
loader.lazyRequireGetter(this, "CATEGORY_JIT",
  "devtools/shared/profiler/global", true);
loader.lazyRequireGetter(this, "JITOptimizations",
  "devtools/shared/profiler/jit", true);
loader.lazyRequireGetter(this, "FrameUtils",
  "devtools/shared/profiler/frame-utils");

exports.ThreadNode = ThreadNode;
exports.FrameNode = FrameNode;
exports.FrameNode.isContent = FrameUtils.isContent;

/**
 * A call tree for a thread. This is essentially a linkage between all frames
 * of all samples into a single tree structure, with additional information
 * on each node, like the time spent (in milliseconds) and samples count.
 *
 * Example:
 * {
 *   duration: number,
 *   calls: {
 *     "FunctionName (url:line)": {
 *       line: number,
 *       category: number,
 *       samples: number,
 *       duration: number,
 *       calls: {
 *         ...
 *       }
 *     }, // FrameNode
 *     ...
 *   }
 * } // ThreadNode
 *
 * @param object threadSamples
 *        The raw samples array received from the backend.
 * @param object options
 *        Additional supported options, @see ThreadNode.prototype.insert
 *          - number startTime [optional]
 *          - number endTime [optional]
 *          - boolean contentOnly [optional]
 *          - boolean invertTree [optional]
 *          - object optimizations [optional]
 *            The raw tracked optimizations array received from the backend.
 */
function ThreadNode(threadSamples, options = {}) {
  this.samples = 0;
  this.duration = 0;
  this.calls = {};
  this._previousSampleTime = 0;

  for (let sample of threadSamples) {
    this.insert(sample, options);
  }
}

ThreadNode.prototype = {
  /**
   * Adds function calls in the tree from a sample's frames.
   *
   * @param object sample
   *        The { frames, time } sample, containing an array of frames and
   *        the time the sample was taken. This sample is assumed to be older
   *        than the most recently inserted one.
   * @param object options [optional]
   *        Additional supported options:
   *          - number startTime: the earliest sample to start at (in milliseconds)
   *          - number endTime: the latest sample to end at (in milliseconds)
   *          - boolean contentOnly: if platform frames shouldn't be used
   *          - boolean invertTree: if the call tree should be inverted
   *          - object optimizations: The array of all indexable optimizations from the backend.
   */
  insert: function(sample, options = {}) {
    let startTime = options.startTime || 0;
    let endTime = options.endTime || Infinity;
    let optimizations = options.optimizations;
    let sampleTime = sample.time;
    if (!sampleTime || sampleTime < startTime || sampleTime > endTime) {
      return;
    }

    let sampleFrames = sample.frames;

    if (!options.invertTree) {
      // Remove the (root) node if the tree is not inverted: we will synthesize
      // our own root in the view. However, for inverted trees, we wish to be
      // able to differentiate between (root)->A->B->C and (root)->B->C stacks,
      // so we need the root node in that case.
      sampleFrames = sampleFrames.slice(1);
    }

    // Filter out platform frames if only content-related function calls
    // should be taken into consideration.
    if (options.contentOnly) {
      sampleFrames = FrameUtils.filterPlatformData(sampleFrames);
    }

    // If no frames remain after filtering, then this is a leaf node, no need
    // to continue.
    if (!sampleFrames.length) {
      return;
    }
    // Invert the tree after filtering, if preferred.
    if (options.invertTree) {
      sampleFrames.reverse();
    }

    let sampleDuration = sampleTime - this._previousSampleTime;
    this._previousSampleTime = sampleTime;
    this.samples++;
    this.duration += sampleDuration;

    FrameNode.prototype.insert(
      sampleFrames, optimizations, 0, sampleTime, sampleDuration, this.calls);
  },

  /**
   * Gets additional details about this node.
   * @return object
   */
  getInfo: function() {
    return {
      nodeType: "Thread",
      functionName: L10N.getStr("table.root"),
      categoryData: {}
    };
  },

  /**
   * Mimicks the interface of FrameNode, and a ThreadNode can never have
   * optimization data (at the moment, anyway), so provide a function
   * to return null so we don't need to check if a frame node is a thread
   * or not everytime we fetch optimization data.
   *
   * @return {null}
   */

  hasOptimizations: function () {
    return null;
  }
};

/**
 * A function call node in a tree.
 *
 * @param string location
 *        The location of this function call. Note that this isn't sanitized,
 *        so it may very well (not?) include the function name, url, etc.
 * @param number line
 *        The line number inside the source containing this function call.
 * @param number column
 *        The column number inside the source containing this function call.
 * @param number category
 *        The category type of this function call ("js", "graphics" etc.).
 * @param number allocations
 *        The number of memory allocations performed in this frame.
 * @param boolean isMetaCategory
 *        Whether or not this is a platform node that should appear as a
 *        generalized meta category or not.
 */
function FrameNode({ location, line, column, category, allocations, isMetaCategory }) {
  this.location = location;
  this.line = line;
  this.column = column;
  this.category = category;
  this.allocations = allocations || 0;
  this.sampleTimes = [];
  this.samples = 0;
  this.duration = 0;
  this.calls = {};
  this._optimizations = null;
  this.isMetaCategory = isMetaCategory;
}

FrameNode.prototype = {
  /**
   * Adds function calls in the tree from a sample's frames. For example, given
   * the the frames below (which would account for three calls to `insert` on
   * the root frame), the following tree structure is created:
   *
   *                          A
   *   A -> B -> C           / \
   *   A -> B -> D    ~>    B   E
   *   A -> E -> F         / \   \
   *                      C   D   F
   * @param frames
   *        The sample call stack.
   * @param optimizations
   *        The array of indexable optimizations.
   * @param index
   *        The index of the call in the stack representing this node.
   * @param number time
   *        The delta time (in milliseconds) when the frame was sampled.
   * @param number duration
   *        The amount of time spent executing all functions on the stack.
   */
  insert: function(frames, optimizations, index, time, duration, _store = this.calls) {
    let frame = frames[index];
    if (!frame) {
      return;
    }
    // If we are only displaying content, then platform data will have
    // a `isMetaCategory` property. Group by category (GC, Graphics, etc.)
    // to group together frames so they're displayed only once, since we don't
    // need the location anyway.
    let key = frame.isMetaCategory ? frame.category : frame.location;
    let child = _store[key] || (_store[key] = new FrameNode(frame));
    child.sampleTimes.push({ start: time, end: time + duration });
    child.samples++;
    child.duration += duration;
    if (optimizations && frame.optsIndex != null) {
      let opts = child._optimizations || (child._optimizations = new JITOptimizations(optimizations));
      opts.addOptimizationSite(frame.optsIndex);
    }
    child.insert(frames, optimizations, index + 1, time, duration);
  },

  /**
   * Returns the parsed location and additional data describing
   * this frame. Uses cached data if possible.
   *
   * @return object
   *         The computed { name, file, url, line } properties for this
   *         function call.
   */
  getInfo: function() {
    return this._data || this._computeInfo();
  },

  /**
   * Parses the raw location of this function call to retrieve the actual
   * function name and source url.
   */
  _computeInfo: function() {
    // "EnterJIT" pseudoframes are special, not actually on the stack.
    if (this.location == "EnterJIT") {
      this.category = CATEGORY_JIT;
    }

    // Since only C++ stack frames have associated category information,
    // default to an "unknown" category otherwise.
    let categoryData = CATEGORY_MAPPINGS[this.category] || {};

    let parsedData = FrameUtils.parseLocation(this);
    parsedData.nodeType = "Frame";
    parsedData.categoryData = categoryData;
    parsedData.isContent = FrameUtils.isContent(this);
    parsedData.isMetaCategory = this.isMetaCategory;

    return this._data = parsedData;
  },

  /**
   * Returns whether or not the frame node has an JITOptimizations model.
   *
   * @return {Boolean}
   */
  hasOptimizations: function () {
    return !!this._optimizations;
  },

  /**
   * Returns the underlying JITOptimizations model representing
   * the optimization attempts occuring in this frame.
   *
   * @return {JITOptimizations|null}
   */
  getOptimizations: function () {
    return this._optimizations;
  }
};
