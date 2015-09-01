var prefix = "/tests/dom/workers/test/serviceworkers/fetch/origin/";

self.addEventListener("install", function(event) {
  event.waitUntil(
    self.caches.open("origin-cache")
      .then(c => {
        return Promise.all(
          [
            c.add(new Request(prefix + 'index.sjs',
                              { redirect: 'manual' } )),
            c.add(new Request(prefix + 'index-to-https.sjs',
                              { redirect: 'manual' } ))
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
