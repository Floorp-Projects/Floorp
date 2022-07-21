"use strict";

/**
 * Bug 1778239 - Dismiss the "optimized for Chrome" banner on m.pji.co.kr
 *
 * This interventions force-sets a window variable `flag` to true, to bypass
 * the need to use a Chrome user-agent string to hide the banner.
 */

/* globals exportFunction */

Object.defineProperty(window.wrappedJSObject, "flag", {
  get: exportFunction(function() {
    return true;
  }, window),

  set: exportFunction(function() {}, window),
});
