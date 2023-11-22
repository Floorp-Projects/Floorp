"use strict";

oninstall = function (event) {
  event.waitUntil(
    caches.open("perftest").then(function (cache) {
      return cache.put("cached.txt", new Response("cached.txt"));
    })
  );
};

onfetch = function (event) {
  if (event.request.url.endsWith("/cached.txt")) {
    var p = caches.match("cached.txt", { cacheName: "perftest" });
    event.respondWith(p);
  } else if (event.request.url.endsWith("/uncached.txt")) {
    event.respondWith(new Response("uncached.txt"));
  }
};
