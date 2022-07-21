"use strict";

/**
 * Bug 1772949 - Disable WebShare on YouTube embed frames
 *
 * This interventions undefines navigator.share for YouTube embed frames,
 * so they show the traditional YouTube sharing interface instead of nothing.
 */

if (window !== window.top) {
  Object.defineProperty(navigator.wrappedJSObject, "share", {
    value: undefined,
  });
}
