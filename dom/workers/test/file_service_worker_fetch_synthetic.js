addEventListener("install", function(evt) {
  evt.waitUntil(self.skipWaiting());
});

self.addEventListener("fetch", event => {
  event.respondWith(
    new Response(
      `<!DOCTYPE HTML><body>
        <h1 id="url">${event.request.url}</h1>
        <h1 id="location">${self.location.href}</h1>
      </body>`,
      { headers: { "Content-Type": "text/html" } }
    )
  );
});
