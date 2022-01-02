self.addEventListener("install", function(event) {
  event.waitUntil(
    caches.open("cache").then(function(cache) {
      var synth = new Response(
        '<!DOCTYPE html><script>window.parent.postMessage({status: "done-synth-sw"}, "*");</script>',
        { headers: { "Content-Type": "text/html" } }
      );
      return Promise.all([
        cache.add("index.html"),
        cache.put("synth-sw.html", synth),
      ]);
    })
  );
});

self.addEventListener("fetch", function(event) {
  if (event.request.url.includes("index.html")) {
    event.respondWith(caches.match(event.request));
  } else if (event.request.url.includes("synth-sw.html")) {
    event.respondWith(caches.match(event.request));
  } else if (event.request.url.includes("synth-window.html")) {
    event.respondWith(caches.match(event.request));
  } else if (event.request.url.includes("synth.html")) {
    event.respondWith(
      new Response(
        '<!DOCTYPE html><script>window.parent.postMessage({status: "done-synth"}, "*");</script>',
        { headers: { "Content-Type": "text/html" } }
      )
    );
  }
});
