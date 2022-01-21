"use strict";

/**
 * Bug 1724868 - news.yahoo.co.jp - Override UA
 * WebCompat issue #82605 - https://webcompat.com/issues/82605
 *
 * Yahoo Japan news doesn't allow playing video in Firefox on Android
 * as they don't have it in their support matrix. They check UA override twice
 * and display different ui with the same error. Changing UA to Chrome via
 * content script allows playing the videos.
 */

/* globals exportFunction */

console.info(
  "The user agent has been overridden for compatibility reasons. See https://webcompat.com/issues/82605 for details."
);

Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
  get: exportFunction(function() {
    return "Mozilla/5.0 (Linux; Android 11; Pixel 4a) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.159 Mobile Safari/537.36";
  }, window),

  set: exportFunction(function() {}, window),
});
