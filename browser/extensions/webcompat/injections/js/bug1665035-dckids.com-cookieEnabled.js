"use strict";

/**
 * Bug 1665035 - enable navigator.cookieEnabled and spoof window.navigator on Linux
 *
 * Some of the games are not starting because navigator.cookieEnabled
 * returns false for trackers with ETP strict. Overwriting the value allows
 * to play the games. In addition, Linux desktop devices are incorrectly
 * flagged as mobile devices (even if ETP is disabled), so spoofing
 * window.navigator.platform here.
 */

console.info(
  "window.cookieEnabled has been overwritten for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1665035 for details."
);

Object.defineProperty(window.navigator.wrappedJSObject, "cookieEnabled", {
  value: true,
  writable: false,
});

if (navigator.platform.includes("Linux")) {
  console.info(
    "navigator.platform has been overwritten for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1665035 for details."
  );
  Object.defineProperty(window.navigator.wrappedJSObject, "platform", {
    value: "Win64",
    writable: false,
  });
}
