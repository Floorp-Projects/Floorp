/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { isFreetextMatch } = require("./filter-text-utils");

/**
 * Predicates used when filtering items.
 *
 * @param object item
 *        The filtered item.
 * @return boolean
 *         True if the item should be visible, false otherwise.
 */
function all() {
  return true;
}

function isHtml({ mimeType }) {
  return mimeType && mimeType.includes("/html");
}

function isCss({ mimeType }) {
  return mimeType && mimeType.includes("/css");
}

function isJs({ mimeType }) {
  return mimeType && (
    mimeType.includes("/ecmascript") ||
    mimeType.includes("/javascript") ||
    mimeType.includes("/x-javascript"));
}

function isXHR(item) {
  // Show the request it is XHR, except if the request is a WS upgrade
  return item.isXHR && !isWS(item);
}

function isFont({ url, mimeType }) {
  // Fonts are a mess.
  return (mimeType && (
      mimeType.includes("font/") ||
      mimeType.includes("/font"))) ||
    url.includes(".eot") ||
    url.includes(".ttf") ||
    url.includes(".otf") ||
    url.includes(".woff");
}

function isImage({ mimeType }) {
  return mimeType && mimeType.includes("image/");
}

function isMedia({ mimeType }) {
  // Not including images.
  return mimeType && (
    mimeType.includes("audio/") ||
    mimeType.includes("video/") ||
    mimeType.includes("model/") ||
    mimeType === "application/vnd.apple.mpegurl" ||
    mimeType === "application/x-mpegurl");
}

function isWS({ requestHeaders, responseHeaders, cause }) {
  // For the first call, the requestHeaders is not ready(empty),
  // so checking for cause.type instead (Bug-1454962)
  if (typeof cause.type === "string" && cause.type === "websocket") {
    return true;
  }
  // Detect a websocket upgrade if request has an Upgrade header with value 'websocket'
  if (!requestHeaders || !Array.isArray(requestHeaders.headers)) {
    return false;
  }

  // Find the 'upgrade' header.
  let upgradeHeader = requestHeaders.headers.find(header => {
    return (header.name.toLowerCase() == "upgrade");
  });

  // If no header found on request, check response - mainly to get
  // something we can unit test, as it is impossible to set
  // the Upgrade header on outgoing XHR as per the spec.
  if (!upgradeHeader && responseHeaders &&
      Array.isArray(responseHeaders.headers)) {
    upgradeHeader = responseHeaders.headers.find(header => {
      return (header.name.toLowerCase() == "upgrade");
    });
  }

  // Return false if there is no such header or if its value isn't 'websocket'.
  if (!upgradeHeader || upgradeHeader.value != "websocket") {
    return false;
  }

  return true;
}

function isOther(item) {
  const tests = [isHtml, isCss, isJs, isXHR, isFont, isImage, isMedia, isWS];
  return tests.every(is => !is(item));
}

module.exports = {
  Filters: {
    all: all,
    html: isHtml,
    css: isCss,
    js: isJs,
    xhr: isXHR,
    fonts: isFont,
    images: isImage,
    media: isMedia,
    ws: isWS,
    other: isOther,
  },
  isFreetextMatch,
};
