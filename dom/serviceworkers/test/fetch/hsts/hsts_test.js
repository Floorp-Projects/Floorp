self.addEventListener("fetch", function (event) {
  if (event.request.url.includes("index.html")) {
    event.respondWith(fetch("realindex.html"));
  } else if (event.request.url.includes("image-20px.png")) {
    if (event.request.url.indexOf("https://") == 0) {
      event.respondWith(fetch("image-40px.png"));
    } else {
      event.respondWith(Response.error());
    }
  }
});
