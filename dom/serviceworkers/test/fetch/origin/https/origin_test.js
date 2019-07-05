var prefix = "/tests/dom/serviceworkers/test/fetch/origin/https/";

function addOpaqueRedirect(cache, file) {
  return fetch(new Request(prefix + file, { redirect: "manual" })).then(
    function(response) {
      return cache.put(prefix + file, response);
    }
  );
}

self.addEventListener("install", function(event) {
  event.waitUntil(
    self.caches.open("origin-cache").then(c => {
      return addOpaqueRedirect(c, "index-https.sjs");
    })
  );
});

self.addEventListener("fetch", function(event) {
  if (event.request.url.includes("index-cached-https.sjs")) {
    event.respondWith(
      self.caches.open("origin-cache").then(c => {
        return c.match(prefix + "index-https.sjs");
      })
    );
  } else {
    event.respondWith(fetch(event.request));
  }
});
