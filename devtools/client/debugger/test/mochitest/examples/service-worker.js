/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

dump("Starting\n");

self.addEventListener("activate", event => {
  dump("Activate\n");
  event.waitUntil(self.clients.claim());
});

self.addEventListener("fetch", event => {
  const url = event.request.url;
  dump(`Fetch: ${url}\n`);
  if (url.includes("whatever")) {
    const response = new Response("Service worker response", { statusText: "OK" });
    event.respondWith(response);
  }
});
