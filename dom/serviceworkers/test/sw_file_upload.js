self.skipWaiting();

addEventListener("fetch", event => {
  const url = new URL(event.request.url);
  const params = new URLSearchParams(url.search);

  if (params.get("clone") === "1") {
    event.respondWith(fetch(event.request.clone()));
  } else {
    event.respondWith(fetch(event.request));
  }
});

addEventListener("activate", function (event) {
  event.waitUntil(clients.claim());
});
