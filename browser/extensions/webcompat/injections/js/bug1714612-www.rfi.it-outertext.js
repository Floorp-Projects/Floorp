"use strict";

/**
 * Bug 1714612 - Build site patch for www.rfi.it
 * WebCompat issue #155672 - https://webcompat.com/issues/55672
 *
 * This site patch polyfills HTML outerText, as the page relies on it to function
 * properly (the map will not pan properly after zooming in).
 */

/* globals exportFunction */

console.info(
  "HTML outerText has been polyfilled for compatibility reasons. See https://webcompat.com/issues/55672 for details."
);

Object.defineProperty(
  window.wrappedJSObject.HTMLElement.prototype,
  "outerText",
  {
    get: exportFunction(function() {
      return this.innerText;
    }, window),

    set: exportFunction(function(value) {
      this.replaceWith(document.createTextNode(value));
    }, window),
  }
);
