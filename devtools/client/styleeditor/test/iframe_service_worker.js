"use strict";

self.onfetch = function (event) {
  if (event.request.url.includes("sheet.css")) {
    return event.respondWith(new Response("* { color: green; }"));
  }
  return null;
};

self.onactivate =  function (event) {
  event.waitUntil(self.clients.claim());
};
