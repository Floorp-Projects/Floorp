/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

self.addEventListener("activate", event => {
  event.waitUntil(self.clients.claim());
});

self.onfetch = function (event) {
  const url = event.request.url;

  const response = url.endsWith("test")
    ? new Response("lorem ipsum", { statusText: "OK" })
    : fetch(event.request);

  event.respondWith(response);
};
