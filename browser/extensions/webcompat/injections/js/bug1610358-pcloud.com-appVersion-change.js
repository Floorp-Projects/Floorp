"use strict";

/**
 * Bug 1610358 - Add "mobile" to navigator.appVersion
 * WebCompat issue #40353 - https://webcompat.com/issues/40353
 *
 * the site expecting navigator.appVersion to contain "mobile",
 * otherwise it's serving a tablet version for Firefox mobile
 */

/* globals exportFunction */

console.info(
  "The user agent has been overridden for compatibility reasons. See https://webcompat.com/issues/40353 for details."
);

const APP_VERSION = navigator.appVersion + " mobile";

Object.defineProperty(window.navigator.wrappedJSObject, "appVersion", {
  get: exportFunction(function() {
    return APP_VERSION;
  }, window),

  set: exportFunction(function() {}, window),
});
