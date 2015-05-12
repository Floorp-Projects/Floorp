/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const { extend } = require("sdk/util/object");
loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "CATEGORY_OTHER",
  "devtools/shared/profiler/global", true);

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
const CHAR_CODE_COLON = ":".charCodeAt(0);
const CHAR_CODE_SLASH = "/".charCodeAt(0);

// The cache used in the `nsIURL` function.
const gNSURLStore = new Map();

// The cache used to store inflated frames.
const gInflatedFrameStore = new WeakMap();

/**
 * Parses the raw location of this function call to retrieve the actual
 * function name, source url, host name, line and column.
 */
exports.parseLocation = function parseLocation(location, fallbackLine, fallbackColumn) {
  // Parse the `location` for the function name, source url, line, column etc.

  let line, column, url;

  // These two indices are used to extract the resource substring, which is
  // location[firstParenIndex + 1 .. lineAndColumnIndex].
  //
  // The resource substring is extracted iff a line number was found. There
  // may be no parentheses, in which case the substring starts at 0.
  //
  // For example, take "foo (bar.js:1)".
  //                        ^      ^
  //                        |      -----+
  //                        +-------+   |
  //                                |   |
  // firstParenIndex will point to -+   |
  //                                    |
  // lineAndColumnIndex will point to --+
  //
  // For an example without parentheses, take "bar.js:2".
  //                                          ^      ^
  //                                          |      |
  // firstParenIndex will point to -----------+      |
  //                                                 |
  // lineAndColumIndex will point to ----------------+
  let firstParenIndex = -1;
  let lineAndColumnIndex = -1;

  // Compute firstParenIndex and lineAndColumnIndex. If lineAndColumnIndex is
  // found, also extract the line and column.
  for (let i = 0; i < location.length; i++) {
    let c = location.charCodeAt(i);

    // The url and line information might be inside parentheses.
    if (c === CHAR_CODE_LPAREN) {
      if (firstParenIndex < 0) {
        firstParenIndex = i;
      }
      continue;
    }

    // Look for numbers after colons, twice. Firstly for the line, secondly
    // for the column.
    if (c === CHAR_CODE_COLON) {
      if (isNumeric(location.charCodeAt(i + 1))) {
        // If we found a line number, remember when it starts.
        if (lineAndColumnIndex < 0) {
          lineAndColumnIndex = i;
        }

        let start = ++i;
        let length = 1;
        while (isNumeric(location.charCodeAt(++i))) {
          length++;
        }

        // Discard port numbers
        if (location.charCodeAt(i) === CHAR_CODE_SLASH) {
          lineAndColumnIndex = -1;
          --i;
          continue;
        }

        if (!line) {
          line = location.substr(start, length);

          // Unwind a character due to the isNumeric loop above.
          --i;

          // There still might be a column number, continue looking.
          continue;
        }

        column = location.substr(start, length);

        // We've gotten both a line and a column, stop looking.
        break;
      }
    }
  }

  let uri;
  if (lineAndColumnIndex > 0) {
    let resource = location.substring(firstParenIndex + 1, lineAndColumnIndex);
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
    functionName = location.substring(0, firstParenIndex - 1);
    fileName = (uri.fileName + (uri.ref ? "#" + uri.ref : "")) || "/";
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
 * Checks if the specified function represents a chrome or content frame.
 *
 * @param string location
 *        The location of the frame.
 * @param number category [optional]
 *        If a chrome frame, the category.
 * @return boolean
 *         True if a content frame, false if a chrome frame.
 */
function isContent({ location, category }) {
  // Only C++ stack frames have associated category information.
  if (category) {
    return false;
  }

  // Locations in frames with function names look like:
  //   "functionName (foo://bar)".
  // Look for the starting left parenthesis, then try to match a
  // scheme name.
  for (let i = 0; i < location.length; i++) {
    if (location.charCodeAt(i) === CHAR_CODE_LPAREN) {
      return isContentScheme(location, i + 1);
    }
  }

  // If there was no left parenthesis, try matching from the start.
  return isContentScheme(location, 0);
}
exports.isContent = isContent;

/**
 * Get caches to cache inflated frames and computed frame keys of a frame
 * table.
 *
 * @param object framesTable
 * @return object
 */
exports.getInflatedFrameCache = function getInflatedFrameCache(frameTable) {
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
 * @param object allocationsTable
 */
exports.getOrAddInflatedFrame = function getOrAddInflatedFrame(cache, index, frameTable, stringTable, allocationsTable) {
  let inflatedFrame = cache[index];
  if (inflatedFrame === null) {
    inflatedFrame = cache[index] = new InflatedFrame(index, frameTable, stringTable, allocationsTable);
  }
  return inflatedFrame;
};

/**
 * An intermediate data structured used to hold inflated frames.
 *
 * @param number index
 * @param object frameTable
 * @param object stringTable
 * @param object allocationsTable
 */
function InflatedFrame(index, frameTable, stringTable, allocationsTable) {
  const LOCATION_SLOT = frameTable.schema.location;
  const OPTIMIZATIONS_SLOT = frameTable.schema.optimizations;
  const LINE_SLOT = frameTable.schema.line;
  const CATEGORY_SLOT = frameTable.schema.category;

  let frame = frameTable.data[index];
  let category = frame[CATEGORY_SLOT];
  this.location = stringTable[frame[LOCATION_SLOT]];
  this.optimizations = frame[OPTIMIZATIONS_SLOT];
  this.line = frame[LINE_SLOT];
  this.column = undefined;
  this.category = category;
  this.metaCategory = category || CATEGORY_OTHER;
  this.allocations = allocationsTable ? allocationsTable[index] : 0;
  this.isContent = isContent(this);
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
    return this.metaCategory;
  }

  // Return an empty string denoting that this frame should be skipped.
  return "";
};

exports.InflatedFrame = InflatedFrame;

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
