"use strict";

/**
 * Bug 1774005 - Generic window.InstallTrigger shim
 *
 * This interventions shims window.InstallTrigger to a string, which evaluates
 * as `true` in web developers browser sniffing code. This intervention will
 * be applied to multiple domains, see bug 1774005 for more information.
 */

/* globals exportFunction */

console.info(
  "The InstallTrigger has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1774005 for details."
);

Object.defineProperty(window.wrappedJSObject, "InstallTrigger", {
  get: exportFunction(function() {
    return "This property has been shimed for Web Compatibility reasons.";
  }, window),
  set: exportFunction(function(_) {}, window),
});
