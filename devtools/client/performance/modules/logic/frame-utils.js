/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "global",
  "devtools/client/performance/modules/global");

// Character codes used in various parsing helper functions.
const CHAR_CODE_A = "a".charCodeAt(0);
const CHAR_CODE_C = "c".charCodeAt(0);
const CHAR_CODE_E = "e".charCodeAt(0);
const CHAR_CODE_F = "f".charCodeAt(0);
const CHAR_CODE_H = "h".charCodeAt(0);
const CHAR_CODE_I = "i".charCodeAt(0);
const CHAR_CODE_J = "j".charCodeAt(0);
const CHAR_CODE_L = "l".charCodeAt(0);
const CHAR_CODE_M = "m".charCodeAt(0);
const CHAR_CODE_O = "o".charCodeAt(0);
const CHAR_CODE_P = "p".charCodeAt(0);
const CHAR_CODE_R = "r".charCodeAt(0);
const CHAR_CODE_S = "s".charCodeAt(0);
const CHAR_CODE_T = "t".charCodeAt(0);
const CHAR_CODE_U = "u".charCodeAt(0);
const CHAR_CODE_0 = "0".charCodeAt(0);
const CHAR_CODE_9 = "9".charCodeAt(0);

const CHAR_CODE_LPAREN = "(".charCodeAt(0);
const CHAR_CODE_RPAREN = ")".charCodeAt(0);
const CHAR_CODE_COLON = ":".charCodeAt(0);
const CHAR_CODE_SLASH = "/".charCodeAt(0);
const CHAR_CODE_SPACE = " ".charCodeAt(0);

// The cache used in the `nsIURL` function.
const gNSURLStore = new Map();

// The cache used to store inflated frames.
const gInflatedFrameStore = new WeakMap();

// The cache used to store frame data from `getInfo`.
const gFrameData = new WeakMap();

/**
 * Parses the raw location of this function call to retrieve the actual
 * function name, source url, host name, line and column.
 */
function parseLocation(location, fallbackLine, fallbackColumn) {
  // Parse the `location` for the function name, source url, line, column etc.

  let line, column, url;

  // These two indices are used to extract the resource substring, which is
  // location[parenIndex + 1 .. lineAndColumnIndex].
  //
  // There are 3 variants of location strings in the profiler (with optional
  // column numbers):
  //   1) "name (resource:line)"
  //   2) "resource:line"
  //   3) "resource"
  //
  // For example for (1), take "foo (bar.js:1)".
  //                                ^      ^
  //                                |      |
  //                                |      |
  //                                |      |
  // parenIndex will point to ------+      |
  //                                       |
  // lineAndColumnIndex will point to -----+
  //
  // For an example without parentheses, take "bar.js:2".
  //                                          ^      ^
  //                                          |      |
  // parenIndex will point to ----------------+      |
  //                                                 |
  // lineAndColumIndex will point to ----------------+
  //
  // To parse, we look for the last occurrence of the string ' ('.
  //
  // For 1), all occurrences of space ' ' characters in the resource string
  // are urlencoded, so the last occurrence of ' (' is the separator between
  // the function name and the resource.
  //
  // For 2) and 3), there can be no occurences of ' (' since ' ' characters
  // are urlencoded in the resource string.
  //
  // XXX: Note that 3) is ambiguous with SPS marker locations like
  // "EnterJIT". We can't distinguish the two, so we treat 3) like a function
  // name.
  let parenIndex = -1;
  let lineAndColumnIndex = -1;

  let lastCharCode = location.charCodeAt(location.length - 1);
  let i;
  if (lastCharCode === CHAR_CODE_RPAREN) {
    // Case 1)
    i = location.length - 2;
  } else if (isNumeric(lastCharCode)) {
    // Case 2)
    i = location.length - 1;
  } else {
    // Case 3)
    i = 0;
  }

  if (i !== 0) {
    // Look for a :number.
    let end = i;
    while (isNumeric(location.charCodeAt(i))) {
      i--;
    }
    if (location.charCodeAt(i) === CHAR_CODE_COLON) {
      column = location.substr(i + 1, end - i);
      i--;
    }

    // Look for a preceding :number.
    end = i;
    while (isNumeric(location.charCodeAt(i))) {
      i--;
    }

    // If two were found, the first is the line and the second is the
    // column. If only a single :number was found, then it is the line number.
    if (location.charCodeAt(i) === CHAR_CODE_COLON) {
      line = location.substr(i + 1, end - i);
      lineAndColumnIndex = i;
      i--;
    } else {
      lineAndColumnIndex = i + 1;
      line = column;
      column = undefined;
    }
  }

  // Look for the last occurrence of ' (' in case 1).
  if (lastCharCode === CHAR_CODE_RPAREN) {
    for (; i >= 0; i--) {
      if (location.charCodeAt(i) === CHAR_CODE_LPAREN &&
          i > 0 &&
          location.charCodeAt(i - 1) === CHAR_CODE_SPACE) {
        parenIndex = i;
        break;
      }
    }
  }

  let uri;
  if (lineAndColumnIndex > 0) {
    let resource = location.substring(parenIndex + 1, lineAndColumnIndex);
    url = resource.split(" -> ").pop();
    if (url) {
      uri = nsIURL(url);
    }
  }

  let functionName, fileName, hostName, port, host;
  line = line || fallbackLine;
  column = column || fallbackColumn;

  // If the URI digged out from the `location` is valid, this is a JS frame.
  if (uri) {
    functionName = location.substring(0, parenIndex - 1);
    fileName = uri.fileName || "/";
    hostName = getHost(url, uri.host);
    // nsIURL throws when accessing a piece of a URL that doesn't
    // exist, because we can't have nice things. Only check this if hostName
    // exists, to save an extra try/catch.
    if (hostName) {
      try {
        port = uri.port === -1 ? null : uri.port;
        host = port !== null ? `${hostName}:${port}` : hostName;
      } catch (e) {
        host = hostName;
      }
    }
  } else {
    functionName = location;
    url = null;
  }

  return { functionName, fileName, hostName, host, port, url, line, column };
};

/**
 * Sets the properties of `isContent` and `category` on a frame.
 *
 * @param {InflatedFrame} frame
 */
function computeIsContentAndCategory(frame) {
  // Only C++ stack frames have associated category information.
  if (frame.category) {
    return;
  }

  let location = frame.location;

  // There are 3 variants of location strings in the profiler (with optional
  // column numbers):
  //   1) "name (resource:line)"
  //   2) "resource:line"
  //   3) "resource"
  let lastCharCode = location.charCodeAt(location.length - 1);
  let schemeStartIndex = -1;
  if (lastCharCode === CHAR_CODE_RPAREN) {
    // Case 1)
    //
    // Need to search for the last occurrence of ' (' to find the start of the
    // resource string.
    for (let i = location.length - 2; i >= 0; i--) {
      if (location.charCodeAt(i) === CHAR_CODE_LPAREN &&
          i > 0 &&
          location.charCodeAt(i - 1) === CHAR_CODE_SPACE) {
        schemeStartIndex = i + 1;
        break;
      }
    }
  } else {
    // Cases 2) and 3)
    schemeStartIndex = 0;
  }

  if (isContentScheme(location, schemeStartIndex)) {
    frame.isContent = true;
    return;
  }

  if (schemeStartIndex !== 0) {
    for (let j = schemeStartIndex; j < location.length; j++) {
      if (location.charCodeAt(j) === CHAR_CODE_R &&
          isChromeScheme(location, j) &&
          (location.indexOf("resource://gre/modules/devtools") !== -1 ||
           location.indexOf("resource:///modules/devtools") !== -1)) {
        frame.category = global.CATEGORY_DEVTOOLS;
        return;
      }
    }
  }

  if (location === "EnterJIT") {
    frame.category = global.CATEGORY_JIT;
    return;
  }

  frame.category = global.CATEGORY_OTHER;
}

/**
 * Get caches to cache inflated frames and computed frame keys of a frame
 * table.
 *
 * @param object framesTable
 * @return object
 */
function getInflatedFrameCache(frameTable) {
  let inflatedCache = gInflatedFrameStore.get(frameTable);
  if (inflatedCache !== undefined) {
    return inflatedCache;
  }

  // Fill with nulls to ensure no holes.
  inflatedCache = Array.from({ length: frameTable.data.length }, () => null);
  gInflatedFrameStore.set(frameTable, inflatedCache);
  return inflatedCache;
};

/**
 * Get or add an inflated frame to a cache.
 *
 * @param object cache
 * @param number index
 * @param object frameTable
 * @param object stringTable
 */
function getOrAddInflatedFrame(cache, index, frameTable, stringTable) {
  let inflatedFrame = cache[index];
  if (inflatedFrame === null) {
    inflatedFrame = cache[index] = new InflatedFrame(index, frameTable, stringTable);
  }
  return inflatedFrame;
};

/**
 * An intermediate data structured used to hold inflated frames.
 *
 * @param number index
 * @param object frameTable
 * @param object stringTable
 */
function InflatedFrame(index, frameTable, stringTable) {
  const LOCATION_SLOT = frameTable.schema.location;
  const IMPLEMENTATION_SLOT = frameTable.schema.implementation;
  const OPTIMIZATIONS_SLOT = frameTable.schema.optimizations;
  const LINE_SLOT = frameTable.schema.line;
  const CATEGORY_SLOT = frameTable.schema.category;

  let frame = frameTable.data[index];
  let category = frame[CATEGORY_SLOT];
  this.location = stringTable[frame[LOCATION_SLOT]];
  this.implementation = frame[IMPLEMENTATION_SLOT];
  this.optimizations = frame[OPTIMIZATIONS_SLOT];
  this.line = frame[LINE_SLOT];
  this.column = undefined;
  this.category = category;
  this.isContent = false;

  // Attempt to compute if this frame is a content frame, and if not,
  // its category.
  //
  // Since only C++ stack frames have associated category information,
  // attempt to generate a useful category, fallback to the one provided
  // by the profiling data, or fallback to an unknown category.
  computeIsContentAndCategory(this);
};

/**
 * Gets the frame key (i.e., equivalence group) according to options. Content
 * frames are always identified by location. Chrome frames are identified by
 * location if content-only filtering is off. If content-filtering is on, they
 * are identified by their category.
 *
 * @param object options
 * @return string
 */
InflatedFrame.prototype.getFrameKey = function getFrameKey(options) {
  if (this.isContent || !options.contentOnly || options.isRoot) {
    options.isMetaCategoryOut = false;
    return this.location;
  }

  if (options.isLeaf) {
    // We only care about leaf platform frames if we are displaying content
    // only. If no category is present, give the default category of
    // CATEGORY_OTHER.
    //
    // 1. The leaf is where time is _actually_ being spent, so we _need_ to
    // show it to developers in some way to give them accurate profiling
    // data. We decide to split the platform into various category buckets
    // and just show time spent in each bucket.
    //
    // 2. The calls leading to the leaf _aren't_ where we are spending time,
    // but _do_ give the developer context for how they got to the leaf
    // where they _are_ spending time. For non-platform hackers, the
    // non-leaf platform frames don't give any meaningful context, and so we
    // can safely filter them out.
    options.isMetaCategoryOut = true;
    return this.category;
  }

  // Return an empty string denoting that this frame should be skipped.
  return "";
};

/**
 * Helper for getting an nsIURL instance out of a string.
 */
function nsIURL(url) {
  let cached = gNSURLStore.get(url);
  // If we cached a valid URI, or `null` in the case
  // of a failure, return it.
  if (cached !== void 0) {
    return cached;
  }
  let uri = null;
  try {
    uri = Services.io.newURI(url, null, null).QueryInterface(Ci.nsIURL);
    // Access the host, because the constructor doesn't necessarily throw
    // if it's invalid, but accessing the host can throw as well
    uri.host;
  } catch(e) {
    // The passed url string is invalid.
    uri = null;
  }

  gNSURLStore.set(url, uri);
  return uri;
};

/**
 * Takes a `host` string from an nsIURL instance and
 * returns the same string, or null, if it's an invalid host.
 */
function getHost (url, hostName) {
  return isChromeScheme(url, 0) ? null : hostName;
}

// For the functions below, we assume that we will never access the location
// argument out of bounds, which is indeed the vast majority of cases.
//
// They are written this way because they are hot. Each frame is checked for
// being content or chrome when processing the profile.

function isColonSlashSlash(location, i) {
  return location.charCodeAt(++i) === CHAR_CODE_COLON &&
         location.charCodeAt(++i) === CHAR_CODE_SLASH &&
         location.charCodeAt(++i) === CHAR_CODE_SLASH;
}

function isContentScheme(location, i) {
  let firstChar = location.charCodeAt(i);

  switch (firstChar) {
  case CHAR_CODE_H: // "http://" or "https://"
    if (location.charCodeAt(++i) === CHAR_CODE_T &&
        location.charCodeAt(++i) === CHAR_CODE_T &&
        location.charCodeAt(++i) === CHAR_CODE_P) {
      if (location.charCodeAt(i + 1) === CHAR_CODE_S) {
        ++i;
      }
      return isColonSlashSlash(location, i);
    }
    return false;

  case CHAR_CODE_F: // "file://"
    if (location.charCodeAt(++i) === CHAR_CODE_I &&
        location.charCodeAt(++i) === CHAR_CODE_L &&
        location.charCodeAt(++i) === CHAR_CODE_E) {
      return isColonSlashSlash(location, i);
    }
    return false;

  case CHAR_CODE_A: // "app://"
    if (location.charCodeAt(++i) == CHAR_CODE_P &&
        location.charCodeAt(++i) == CHAR_CODE_P) {
      return isColonSlashSlash(location, i);
    }
    return false;

  default:
    return false;
  }
}

function isChromeScheme(location, i) {
  let firstChar = location.charCodeAt(i);

  switch (firstChar) {
  case CHAR_CODE_C: // "chrome://"
    if (location.charCodeAt(++i) === CHAR_CODE_H &&
        location.charCodeAt(++i) === CHAR_CODE_R &&
        location.charCodeAt(++i) === CHAR_CODE_O &&
        location.charCodeAt(++i) === CHAR_CODE_M &&
        location.charCodeAt(++i) === CHAR_CODE_E) {
      return isColonSlashSlash(location, i);
    }
    return false;

  case CHAR_CODE_R: // "resource://"
    if (location.charCodeAt(++i) === CHAR_CODE_E &&
        location.charCodeAt(++i) === CHAR_CODE_S &&
        location.charCodeAt(++i) === CHAR_CODE_O &&
        location.charCodeAt(++i) === CHAR_CODE_U &&
        location.charCodeAt(++i) === CHAR_CODE_R &&
        location.charCodeAt(++i) === CHAR_CODE_C &&
        location.charCodeAt(++i) === CHAR_CODE_E) {
      return isColonSlashSlash(location, i);
    }
    return false;

  case CHAR_CODE_J: // "jar:file://"
    if (location.charCodeAt(++i) === CHAR_CODE_A &&
        location.charCodeAt(++i) === CHAR_CODE_R &&
        location.charCodeAt(++i) === CHAR_CODE_COLON &&
        location.charCodeAt(++i) === CHAR_CODE_F &&
        location.charCodeAt(++i) === CHAR_CODE_I &&
        location.charCodeAt(++i) === CHAR_CODE_L &&
        location.charCodeAt(++i) === CHAR_CODE_E) {
      return isColonSlashSlash(location, i);
    }
    return false;

  default:
    return false;
  }
}

function isNumeric(c) {
  return c >= CHAR_CODE_0 && c <= CHAR_CODE_9;
}

/**
 * Calculates the relative costs of this frame compared to a root,
 * and generates allocations information if specified. Uses caching
 * if possible.
 *
 * @param {ThreadNode|FrameNode} node
 *                               The node we are calculating.
 * @param {ThreadNode} options.root
 *                     The root thread node to calculate relative costs.
 *                     Generates [self|total] [duration|percentage] values.
 * @param {boolean} options.allocations
 *                  Generates `totalAllocations` and `selfAllocations`.
 *
 * @return {object}
 */
function getFrameInfo (node, options) {
  let data = gFrameData.get(node);

  if (!data) {
    if (node.nodeType === "Thread") {
      data = Object.create(null);
      data.functionName = global.L10N.getStr("table.root");
    } else {
      data = parseLocation(node.location, node.line, node.column);
      data.hasOptimizations = node.hasOptimizations();
      data.isContent = node.isContent;
      data.isMetaCategory = node.isMetaCategory;
    }
    data.samples = node.youngestFrameSamples;
    data.categoryData = global.CATEGORY_MAPPINGS[node.category] || {};
    data.nodeType = node.nodeType;

    // Frame name (function location or some meta information)
    data.name = data.isMetaCategory ? data.categoryData.label : data.functionName || "";
    data.tooltiptext = data.isMetaCategory ? data.categoryData.label : node.location || "";

    gFrameData.set(node, data);
  }

  // If no options specified, we can't calculate relative values, abort here
  if (!options) {
    return data;
  }

  // If a root specified, calculate the relative costs in the context of
  // this call tree. The cached store may already have this, but generate
  // if it does not.
  let totalSamples = options.root.samples;
  let totalDuration = options.root.duration;
  if (options && options.root && !data.COSTS_CALCULATED) {
    data.selfDuration = node.youngestFrameSamples / totalSamples * totalDuration;
    data.selfPercentage = node.youngestFrameSamples / totalSamples * 100;
    data.totalDuration = node.samples / totalSamples * totalDuration;
    data.totalPercentage = node.samples / totalSamples * 100;
    data.COSTS_CALCULATED = true;
  }

  if (options && options.allocations && !data.ALLOCATION_DATA_CALCULATED) {
    let totalBytes = options.root.byteSize;
    data.selfCount = node.youngestFrameSamples;
    data.totalCount = node.samples;
    data.selfCountPercentage = node.youngestFrameSamples / totalSamples * 100;
    data.totalCountPercentage = node.samples / totalSamples * 100;
    data.selfSize = node.youngestFrameByteSize;
    data.totalSize = node.byteSize;
    data.selfSizePercentage = node.youngestFrameByteSize / totalBytes * 100;
    data.totalSizePercentage = node.byteSize / totalBytes * 100;
    data.ALLOCATION_DATA_CALCULATED = true;
  }

  return data;
}

exports.getFrameInfo = getFrameInfo;

/**
 * Takes an inverted ThreadNode and searches its youngest frames for
 * a FrameNode with matching location.
 *
 * @param {ThreadNode} threadNode
 * @param {string} location
 * @return {?FrameNode}
 */
function findFrameByLocation (threadNode, location) {
  if (!threadNode.inverted) {
    throw new Error("FrameUtils.findFrameByLocation only supports leaf nodes in an inverted tree.");
  }

  let calls = threadNode.calls;
  for (let i = 0; i < calls.length; i++) {
    if (calls[i].location === location) {
      return calls[i];
    }
  }
  return null;
}

exports.findFrameByLocation = findFrameByLocation;
exports.computeIsContentAndCategory = computeIsContentAndCategory;
exports.parseLocation = parseLocation;
exports.getInflatedFrameCache = getInflatedFrameCache;
exports.getOrAddInflatedFrame = getOrAddInflatedFrame;
exports.InflatedFrame = InflatedFrame;
