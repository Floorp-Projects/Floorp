"use strict";

/**
 * Bug 1625224 - sixt-neuwagen.de - set window.netscape to undefined
 * WebCompat issue #50717 - https://webcompat.com/issues/50717
 *
 * sixt-neuwagen.de uses window.netscape to detect if the current browser
 * is Firefox and then uses obj.toSource(). Since Firefox 74 obj.toSource()
 * is no longer available and causes the site to break
 *
 * This site patch sets window.netscape to undefined
 */

/* globals exportFunction */

console.info(
  "window.netscape has been shimmed for compatibility reasons. See https://webcompat.com/issues/50717 for details."
);

Object.defineProperty(window.wrappedJSObject, "netscape", {
  get: exportFunction(function() {
    return undefined;
  }, window),

  set: exportFunction(function() {}, window),
});
