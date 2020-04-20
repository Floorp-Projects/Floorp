"use strict";

/**
 * Bug 1629926 - mail.google.com - set allow mic on hangouts iframe
 *
 * mail.google.com has a chain of iframes for hangouts "Make a call" interface,
 * with a parent iframe not having allow=microphone attribute. It's resulting
 * in calls not working since feature policy requires allow lists all the way
 * up to the top document.
 *
 * Adding allow=microphone to the iframe in question makes the calls work
 */

/* globals exportFunction */

console.info(
  "hangouts iframe allow property was changed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1503694 for details."
);

const isHangoutsSrc = src => {
  const url = new URL(src);
  return url.host === "hangouts.google.com" && url.pathname.includes("blank");
};

const orig = Object.getOwnPropertyDescriptor(
  HTMLIFrameElement.prototype.wrappedJSObject,
  "src"
);

Object.defineProperty(HTMLIFrameElement.prototype.wrappedJSObject, "src", {
  get: orig.get,
  set: exportFunction(function(val) {
    if (isHangoutsSrc(val)) {
      this.allow = "microphone *; autoplay *; microphone";
    }
    this.src = val;
  }, window),
  configurable: true,
  enumerable: true,
});
