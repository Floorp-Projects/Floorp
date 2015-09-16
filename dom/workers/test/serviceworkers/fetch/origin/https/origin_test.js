var prefix = "/tests/dom/workers/test/serviceworkers/fetch/origin/https/";

self.addEventListener("install", function(event) {
  event.waitUntil(
    self.caches.open("origin-cache")
      .then(c => {
        return c.add(new Request(prefix + 'index-https.sjs', {redirect: "manual"}));
      })
  );
});

self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index-cached-https.sjs") >= 0) {
    event.respondWith(
      self.caches.open("origin-cache")
        .then(c => {
          return c.match(prefix + 'index-https.sjs');
        })
    );
  } else {
    event.respondWith(fetch(event.request));
  }
});
