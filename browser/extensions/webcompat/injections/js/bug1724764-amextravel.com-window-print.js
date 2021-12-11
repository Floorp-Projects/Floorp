"use strict";

/**
 * Bug 1724764 - https://bugzilla.mozilla.org/show_bug.cgi?id=1724764
 * WebCompat issue #81762 - https://webcompat.com/issues/81762
 *
 * Amex travel is not displaying search results due to an error caused
 * by missing window.print() function. Adding print to the window object
 * allows to load the results.
 */

/* globals exportFunction */

console.info(
  "window.print has been shimmed for compatibility reasons. See https://webcompat.com/issues/81762 for details."
);

Object.defineProperty(window.wrappedJSObject, "print", {
  get: exportFunction(function() {
    return true;
  }, window),

  set: exportFunction(function() {}, window),
});
