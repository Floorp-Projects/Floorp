self.skipWaiting();

addEventListener("fetch", event => {
  event.respondWith(fetch(event.request));
});

addEventListener("activate", function(event) {
  event.waitUntil(clients.claim());
});
