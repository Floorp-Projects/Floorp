var prefix = "/tests/dom/workers/test/serviceworkers/fetch/origin/";

self.addEventListener("install", function(event) {
  event.waitUntil(
    self.caches.open("origin-cache")
      .then(c => {
        return c.add(prefix + 'index.sjs');
      })
  );
});

self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index-cached.sjs") >= 0) {
    event.respondWith(
      self.caches.open("origin-cache")
        .then(c => {
          return c.match(prefix + 'index.sjs');
        })
    );
  } else {
    event.respondWith(fetch(event.request));
  }
});
