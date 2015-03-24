self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0) {
    event.respondWith(fetch("intercepted_index.html"));
  }
});
