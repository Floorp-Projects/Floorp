"use strict";

/**
 * Bug 1784309 - Shim Math.pow if Math.PI and exponent "-100" are passed on bet365.com
 *
 * The site is expecting Math.pow to return a certain value if these parameters are passed,
 * otherwise some content parts are missing. Shimming the return value fixes the issue.
 */

/* globals exportFunction */

console.info(
  "Math.pow with arguments of `Math.PI` and exponent `-100` has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1784111 for details."
);

const originalMath = Math;
const originalPow = Math.pow;

Object.defineProperty(window.Math.wrappedJSObject, "pow", {
  value: exportFunction(function(base, exponent) {
    if (exponent === -100 && base === originalMath.PI) {
      return Number("1.9275814160560185e-50");
    }
    return originalPow.call(originalMath, base, exponent);
  }, window),
});
