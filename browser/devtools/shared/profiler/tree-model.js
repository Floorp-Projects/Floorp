/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "L10N",
  "devtools/shared/profiler/global", true);
loader.lazyRequireGetter(this, "CATEGORY_MAPPINGS",
  "devtools/shared/profiler/global", true);
loader.lazyRequireGetter(this, "CATEGORY_JIT",
  "devtools/shared/profiler/global", true);

const CHROME_SCHEMES = ["chrome://", "resource://"];
const CONTENT_SCHEMES = ["http://", "https://", "file://"];

exports.ThreadNode = ThreadNode;
exports.FrameNode = FrameNode;
exports.FrameNode.isContent = isContent;

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
   */
  insert: function(sample, options = {}) {
    let startTime = options.startTime || 0;
    let endTime = options.endTime || Infinity;
    let sampleTime = sample.time;
    if (!sampleTime || sampleTime < startTime || sampleTime > endTime) {
      return;
    }

    let sampleFrames = sample.frames;

    // Filter out platform frames if only content-related function calls
    // should be taken into consideration.
    if (options.contentOnly) {
      // The (root) node is not considered a content function, it'll be removed.
      sampleFrames = sampleFrames.filter(isContent);
    } else {
      // Remove the (root) node manually.
      sampleFrames = sampleFrames.slice(1);
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
      sampleFrames, 0, sampleTime, sampleDuration, this.calls);
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
 */
function FrameNode({ location, line, column, category, allocations }) {
  this.location = location;
  this.line = line;
  this.column = column;
  this.category = category;
  this.allocations = allocations || 0;
  this.sampleTimes = [];
  this.samples = 0;
  this.duration = 0;
  this.calls = {};
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
   * @param index
   *        The index of the call in the stack representing this node.
   * @param number time
   *        The delta time (in milliseconds) when the frame was sampled.
   * @param number duration
   *        The amount of time spent executing all functions on the stack.
   */
  insert: function(frames, index, time, duration, _store = this.calls) {
    let frame = frames[index];
    if (!frame) {
      return;
    }
    let location = frame.location;
    let child = _store[location] || (_store[location] = new FrameNode(frame));
    child.sampleTimes.push({ start: time, end: time + duration });
    child.samples++;
    child.duration += duration;
    child.insert(frames, ++index, time, duration);
  },

  /**
   * Parses the raw location of this function call to retrieve the actual
   * function name and source url.
   *
   * @return object
   *         The computed { name, file, url, line } properties for this
   *         function call.
   */
  getInfo: function() {
    // "EnterJIT" pseudoframes are special, not actually on the stack.
    if (this.location == "EnterJIT") {
      this.category = CATEGORY_JIT;
    }

    // Since only C++ stack frames have associated category information,
    // default to an "unknown" category otherwise.
    let categoryData = CATEGORY_MAPPINGS[this.category] || {};

    // Parse the `location` for the function name, source url, line, column etc.
    let lineAndColumn = this.location.match(/((:\d+)*)\)?$/)[1];
    let [, line, column] = lineAndColumn.split(":");
    line = line || this.line;
    column = column || this.column;

    let firstParenIndex = this.location.indexOf("(");
    let lineAndColumnIndex = this.location.indexOf(lineAndColumn);
    let resource = this.location.substring(firstParenIndex + 1, lineAndColumnIndex);

    let url = resource.split(" -> ").pop();
    let uri = nsIURL(url);
    let functionName, fileName, hostName;

    // If the URI digged out from the `location` is valid, this is a JS frame.
    if (uri) {
      functionName = this.location.substring(0, firstParenIndex - 1);
      fileName = (uri.fileName + (uri.ref ? "#" + uri.ref : "")) || "/";
      hostName = uri.host;
    } else {
      functionName = this.location;
      url = null;
    }

    return {
      nodeType: "Frame",
      functionName: functionName,
      fileName: fileName,
      hostName: hostName,
      url: url,
      line: line,
      column: column,
      categoryData: categoryData,
      isContent: !!isContent(this)
    };
  }
};

/**
 * Checks if the specified function represents a chrome or content frame.
 *
 * @param object frame
 *        The { category, location } properties of the frame.
 * @return boolean
 *         True if a content frame, false if a chrome frame.
 */
function isContent({ category, location }) {
  // Only C++ stack frames have associated category information.
  return !category &&
    !CHROME_SCHEMES.find(e => location.contains(e)) &&
    CONTENT_SCHEMES.find(e => location.contains(e));
}

/**
 * Helper for getting an nsIURL instance out of a string.
 */
function nsIURL(url) {
  let cached = gNSURLStore.get(url);
  if (cached) {
    return cached;
  }
  let uri = null;
  try {
    uri = Services.io.newURI(url, null, null).QueryInterface(Ci.nsIURL);
  } catch(e) {
    // The passed url string is invalid.
  }
  gNSURLStore.set(url, uri);
  return uri;
}

// The cache used in the `nsIURL` function.
let gNSURLStore = new Map();
