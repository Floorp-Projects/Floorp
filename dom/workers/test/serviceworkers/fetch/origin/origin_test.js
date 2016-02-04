var prefix = "/tests/dom/workers/test/serviceworkers/fetch/origin/";

function addOpaqueRedirect(cache, file) {
  return fetch(new Request(prefix + file, { redirect: "manual" })).then(function(response) {
    return cache.put(prefix + file, response);
  });
}

self.addEventListener("install", function(event) {
  event.waitUntil(
    self.caches.open("origin-cache")
      .then(c => {
        return Promise.all(
          [
            addOpaqueRedirect(c, 'index.sjs'),
            addOpaqueRedirect(c, 'index-to-https.sjs')
          ]
        );
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
  } else if (event.request.url.indexOf("index-to-https-cached.sjs") >= 0) {
    event.respondWith(
      self.caches.open("origin-cache")
        .then(c => {
          return c.match(prefix + 'index-to-https.sjs');
        })
    );
  } else {
    event.respondWith(fetch(event.request));
  }
});
