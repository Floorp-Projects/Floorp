/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

exports.getSourcemapBaseURL = getSourcemapBaseURL;
function getSourcemapBaseURL(url, global) {
  let sourceMapBaseURL = null;
  if (url) {
    // Sources that have explicit URLs can be used directly as the base.
    sourceMapBaseURL = url;
  } else if (global?.location?.href) {
    // If there is no URL for the source, the map comment is relative to the
    // page being viewed, so we use the document href.
    sourceMapBaseURL = global?.location?.href;
  } else {
    // If there is no valid base, the sourcemap URL will need to be an absolute
    // URL of some kind.
    return null;
  }

  // A data URL is large and will never be a valid base, so we can just treat
  // it as if there is no base at all to avoid a sending it to the client
  // for no reason.
  if (sourceMapBaseURL.startsWith("data:")) {
    return null;
  }

  // If the base URL is a blob, we want to resolve relative to the origin
  // that created the blob URL, if there is one.
  if (sourceMapBaseURL.startsWith("blob:")) {
    try {
      const parsedBaseURL = new URL(sourceMapBaseURL);
      return parsedBaseURL.origin === "null" ? null : parsedBaseURL.origin;
    } catch (err) {
      return null;
    }
  }

  return sourceMapBaseURL;
}
