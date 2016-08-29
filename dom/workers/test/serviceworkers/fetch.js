addEventListener('fetch', function(event) {
  if (event.request.url.indexOf("fail.html") !== -1) {
    event.respondWith(fetch("serviceworker.html", {"integrity": "abc"}));
  } else if (event.request.url.indexOf("fake.html") !== -1) {
    event.respondWith(fetch("serviceworker.html"));
  } else {
    event.respondWith(new Response("Hello world"));
  }
});

addEventListener("activate", function(event) {
  event.waitUntil(clients.claim());
});
