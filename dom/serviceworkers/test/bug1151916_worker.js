onactivate = function(e) {
  e.waitUntil(self.caches.open("default-cache").then(function(cache) {
    var response = new Response("Hi there");
    return cache.put("madeup.txt", response);
  }));
}

onfetch = function(e) {
  if (e.request.url.match(/madeup.txt$/)) {
    var p = self.caches.match("madeup.txt", { cacheName: "default-cache" });
    e.respondWith(p);
  }
}
