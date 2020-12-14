"use strict";

/*
 * Bug 1682238 - Override navigator.userAgent for gamearter.com on macOS 11.0
 * Bug 1680516 - Game is not loaded on gamearter.com
 *
 * Unity < 2021.1.0a2 is unable to correctly parse User Agents with
 * "Mac OS X 11.0" in them, so let's override to "Mac OS X 10.16" instead
 * for now.
 */

/* globals exportFunction */

if (navigator.userAgent.includes("Mac OS X 11.0;")) {
  console.info(
    "The user agent has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1680516 for details."
  );

  let originalUA = navigator.userAgent;
  Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
    get: exportFunction(function() {
      return originalUA.replace("Mac OS X 11.0;", "Mac OS X 10.16;");
    }, window),

    set: exportFunction(function() {}, window),
  });
}
