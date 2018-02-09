"use strict";

/* globals exportFunction */

Object.defineProperty(window.wrappedJSObject, "isTestFeatureSupported", {
  get: exportFunction(function() {
    return true;
  }, window),

  set: exportFunction(function() {}, window)
});
