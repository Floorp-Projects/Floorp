/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

self.addEventListener("activate", async event => {
  (
    await fetch(
      "https://example.com/browser/devtools/shared/network-observer/test/browser/sjs_network-observer-test-server.sjs?sts=200&fmt=json"
    )
  )
    .json()
    .then(() => console.log("json downloaded"));
  // start controlling the already loaded page
  event.waitUntil(self.clients.claim());
});

self.addEventListener("fetch", event => {
  const response = new Response("Service worker response", {
    statusText: "OK",
  });
  event.respondWith(response);
});
