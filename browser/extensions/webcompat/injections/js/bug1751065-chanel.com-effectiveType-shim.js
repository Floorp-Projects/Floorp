"use strict";

/**
 * Bug 1751065 -  Blank page is loaded due to missing navigator.connection.effectiveType
 * Webcompat issue #89542 - https://github.com/webcompat/web-bugs/issues/89542
 */

/* globals exportFunction */

console.info(
  "navigator.connection.effectiveType has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1751065 for details."
);

Object.defineProperty(
  window.navigator.connection.wrappedJSObject,
  "effectiveType",
  {
    get: exportFunction(function() {
      return "4g";
    }, window),

    set: exportFunction(function() {}, window),
  }
);
