self.addEventListener("fetch", function (event) {
  if (event.request.url.includes("index.html")) {
    event.respondWith(fetch("intercepted_index.html"));
  }
});
