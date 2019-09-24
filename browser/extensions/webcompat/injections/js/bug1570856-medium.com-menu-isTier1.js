"use strict";

/**
 * medium.com - Override window.GLOBALS.useragent.isTier1 to be true
 * WebCompat issue #25844 - https://webcompat.com/issues/25844
 *
 * This site is not showing main menu when scrolling. There is a GLOBALS variable
 * at the bottom of the template being defined based on a server side UA detection.
 * Setting window.GLOBALS.useragent.isTier1 to true makes the menu appear when scrolling
 */

/* globals exportFunction */

console.info(
  "window.GLOBALS.useragent.isTier1 has been set to true for compatibility reasons. See https://webcompat.com/issues/25844 for details."
);

let globals = {};

Object.defineProperty(window.wrappedJSObject, "GLOBALS", {
  get: exportFunction(function() {
    return globals;
  }, window),

  set: exportFunction(function(value = {}) {
    globals = value;

    if (!globals.useragent) {
      globals.useragent = {};
    }

    globals.useragent.isTier1 = true;
  }, window),
});
