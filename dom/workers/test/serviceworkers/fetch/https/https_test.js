self.addEventListener("install", function(event) {
  event.waitUntil(caches.open("cache").then(function(cache) {
    return cache.add("index.html");
  }));
});

self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0) {
    event.respondWith(caches.match(event.request));
  }
});
