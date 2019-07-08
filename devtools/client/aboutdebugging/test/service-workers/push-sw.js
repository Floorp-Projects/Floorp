/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env worker */
/* global clients */

"use strict";

// Send a message to all controlled windows.
function postMessage(message) {
  return clients.matchAll().then(function(clientlist) {
    clientlist.forEach(function(client) {
      client.postMessage(message);
    });
  });
}

// Don't wait for the next page load to become the active service worker.
self.addEventListener("install", function(event) {
  event.waitUntil(self.skipWaiting());
});

// Claim control over the currently open test page when activating.
self.addEventListener("activate", function(event) {
  event.waitUntil(
    self.clients.claim().then(function() {
      return postMessage("sw-claimed");
    })
  );
});

// Forward all "push" events to the controlled window.
self.addEventListener("push", function(event) {
  event.waitUntil(postMessage("sw-pushed"));
});
