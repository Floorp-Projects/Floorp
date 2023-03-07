"use strict";

/**
 * axisbank.com - Shim webkitSpeechRecognition
 * WebCompat issue #117770 - https://webcompat.com/issues/117770
 *
 * The page with bank offerings is not loading options due to the
 * site relying on webkitSpeechRecognition, which is undefined in Firefox.
 * Shimming it to `class {}` makes the pages work.
 */

/* globals exportFunction */

console.info(
  "webkitSpeechRecognition was shimmed for compatibility reasons. See https://webcompat.com/issues/117770 for details."
);

Object.defineProperty(window.wrappedJSObject, "webkitSpeechRecognition", {
  value: exportFunction(function() {
    return class {};
  }, window),
});
