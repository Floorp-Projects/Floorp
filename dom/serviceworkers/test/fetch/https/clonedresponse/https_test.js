self.addEventListener("install", function(event) {
  event.waitUntil(
    caches.open("cache").then(function(cache) {
      return cache.add("index.html");
    })
  );
});

self.addEventListener("fetch", function(event) {
  if (event.request.url.includes("index.html")) {
    event.respondWith(
      new Promise(function(resolve, reject) {
        caches.match(event.request).then(function(response) {
          resolve(response.clone());
        });
      })
    );
  }
});
