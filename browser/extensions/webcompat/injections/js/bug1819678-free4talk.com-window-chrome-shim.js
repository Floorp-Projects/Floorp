"use strict";

/**
 * Bug 1827678 - UA spoof for www.free4talk.com
 *
 * This site is checking for window.chrome, so let's spoof that.
 */

/* globals exportFunction */

console.info(
  "window.chrome has been shimmed for compatibility reasons. See https://github.com/webcompat/web-bugs/issues/77727 for details."
);

Object.defineProperty(window.wrappedJSObject, "chrome", {
  get: exportFunction(function() {
    return true;
  }, window),

  set: exportFunction(function() {}, window),
});
