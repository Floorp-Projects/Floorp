/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const { extend } = require("sdk/util/object");
loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "CATEGORY_OTHER",
  "devtools/shared/profiler/global", true);

// The cache used in the `nsIURL` function.
const gNSURLStore = new Map();
const CHROME_SCHEMES = ["chrome://", "resource://", "jar:file://"];
const CONTENT_SCHEMES = ["http://", "https://", "file://", "app://"];

/**
 * Parses the raw location of this function call to retrieve the actual
 * function name, source url, host name, line and column.
 */
exports.parseLocation = function parseLocation (frame) {
  // Parse the `location` for the function name, source url, line, column etc.
  let lineAndColumn = frame.location.match(/((:\d+)*)\)?$/)[1];
  let [, line, column] = lineAndColumn.split(":");
  line = line || frame.line;
  column = column || frame.column;

  let firstParenIndex = frame.location.indexOf("(");
  let lineAndColumnIndex = frame.location.indexOf(lineAndColumn);
  let resource = frame.location.substring(firstParenIndex + 1, lineAndColumnIndex);

  let url = resource.split(" -> ").pop();
  let uri = nsIURL(url);
  let functionName, fileName, hostName;

  // If the URI digged out from the `location` is valid, this is a JS frame.
  if (uri) {
    functionName = frame.location.substring(0, firstParenIndex - 1);
    fileName = (uri.fileName + (uri.ref ? "#" + uri.ref : "")) || "/";
    hostName = getHost(url, uri.host);
  } else {
    functionName = frame.location;
    url = null;
  }

  return {
    functionName: functionName,
    fileName: fileName,
    hostName: hostName,
    url: url,
    line: line,
    column: column
  };
},

/**
 * Determines if the given frame is the (root) frame.
 *
 * @param object frame
 * @return boolean
 */
exports.isRoot = function isRoot({ location }) {
  return location === "(root)";
};

/**
* Checks if the specified function represents a chrome or content frame.
*
* @param object frame
*        The { category, location } properties of the frame.
* @return boolean
*         True if a content frame, false if a chrome frame.
*/
exports.isContent = function isContent (frame) {
  if (exports.isRoot(frame)) {
    return true;
  }

  // Only C++ stack frames have associated category information.
  const { category, location } = frame;
  return !!(!category &&
    !CHROME_SCHEMES.find(e => location.includes(e)) &&
    CONTENT_SCHEMES.find(e => location.includes(e)));
}

/**
 * This filters out platform data frames in a sample. With latest performance
 * tool in Fx40, when displaying only content, we still filter out all platform data,
 * except we generalize platform data that are leaves. We do this because of two
 * observations:
 *
 * 1. The leaf is where time is _actually_ being spent, so we _need_ to show it
 * to developers in some way to give them accurate profiling data. We decide to
 * split the platform into various category buckets and just show time spent in
 * each bucket.
 *
 * 2. The calls leading to the leaf _aren't_ where we are spending time, but
 * _do_ give the developer context for how they got to the leaf where they _are_
 * spending time. For non-platform hackers, the non-leaf platform frames don't
 * give any meaningful context, and so we can safely filter them out.
 *
 * Example transformations:
 * Before: PlatformA -> PlatformB -> ContentA -> ContentB
 * After:  ContentA -> ContentB
 *
 * Before: PlatformA -> ContentA -> PlatformB -> PlatformC
 * After:  ContentA -> Category(PlatformC)
 */
exports.filterPlatformData = function filterPlatformData (frames) {
  let result = [];
  let last = frames.length - 1;
  let frame;

  for (let i = 0; i < frames.length; i++) {
    frame = frames[i];
    if (exports.isContent(frame)) {
      result.push(frame);
    } else if (last === i) {
      // Extend here so we're not destructively editing
      // the original profiler data. Set isMetaCategory `true`,
      // and ensure we have a category set by default, because that's how
      // the generalized frame nodes are organized.
      result.push(extend({ isMetaCategory: true, category: CATEGORY_OTHER }, frame));
    }
  }

  return result;
}

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
}

/**
 * Takes a `host` string from an nsIURL instance and
 * returns the same string, or null, if it's an invalid host.
 */
function getHost (url, hostName) {
  if (CHROME_SCHEMES.find(e => url.indexOf(e) === 0)) {
    return null;
  }
  return hostName;
}
