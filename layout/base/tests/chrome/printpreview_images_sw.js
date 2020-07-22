self.addEventListener("fetch", event => {
  if (event.request.url.includes("nonexistent.png")) {
    event.respondWith(
      fetch(event.request.url.replace("nonexistent.png", "blue-32x32.png"))
    );
  }
});

self.addEventListener("activate", event => {
  event.waitUntil(clients.claim());
});
