/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

self.addEventListener("activate", event => {
  // start controlling the already loaded page
  event.waitUntil(self.clients.claim());
});

self.addEventListener("fetch", event => {
  const response = new Response("Service worker response");
  event.respondWith(response);
});
