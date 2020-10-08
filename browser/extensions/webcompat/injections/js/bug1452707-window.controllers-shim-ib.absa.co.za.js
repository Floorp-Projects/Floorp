"use strict";

/**
 * Bug 1452707 - Build site patch for ib.absa.co.za
 * WebCompat issue #16401 - https://webcompat.com/issues/16401
 *
 * The online banking at ib.absa.co.za detect if window.controllers is a
 * non-falsy value to detect if the current browser is Firefox or something
 * else. In bug 1448045, this shim has been disabled for Firefox Nightly 61+,
 * which breaks the UA detection on this site and results in a "Browser
 * unsuppored" error message.
 *
 * This site patch simply sets window.controllers to a string, resulting in
 * their check to work again.
 */

/* globals exportFunction */

console.info(
  "window.controllers has been shimmed for compatibility reasons. See https://webcompat.com/issues/16401 for details."
);

Object.defineProperty(window.wrappedJSObject, "controllers", {
  get: exportFunction(function() {
    return true;
  }, window),

  set: exportFunction(function() {}, window),
});
