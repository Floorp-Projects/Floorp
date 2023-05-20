/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Copied from shared-head.js
function waitUntil(predicate, interval = 10) {
  if (predicate()) {
    return Promise.resolve(true);
  }
  return new Promise(resolve => {
    setTimeout(function () {
      waitUntil(predicate, interval).then(() => resolve(true));
    }, interval);
  });
}

// this flag will be flipped externally from controlled-install.html
// by sending a message event to the worker
let canInstall = false;
self.addEventListener("message", event => {
  if (event.data === "install-service-worker") {
    canInstall = true;
  }
});

self.addEventListener("install", event => {
  event.waitUntil(waitUntil(() => canInstall));
});
