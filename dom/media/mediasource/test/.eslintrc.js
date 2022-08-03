"use strict";

module.exports = {
  // Globals from mediasource.js. We use false to indicate they should not
  // be overwritten in scripts.
  globals: {
    addMSEPrefs: false,
    fetchAndLoad: false,
    fetchAndLoadAsync: false,
    fetchWithXHR: false,
    logEvents: false,
    loadSegment: false,
    must_not_reject: false,
    must_not_throw: false,
    must_reject: false,
    must_throw: false,
    once: false,
    range: false,
    runWithMSE: false,
    wait: false,
    waitUntilTime: false,
  },
  // Use const/let instead of var for tighter scoping, avoiding redeclaration
  rules: {
    "no-var": "error",
    "prefer-const": "error",
  },
};
