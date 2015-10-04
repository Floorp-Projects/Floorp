self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0) {
    event.respondWith(fetch("realindex.html"));
  } else if (event.request.url.indexOf("image-20px.png") >= 0) {
    if (event.request.url.indexOf("https://") == 0) {
      event.respondWith(fetch("image-40px.png"));
    } else {
      event.respondWith(Response.error());
    }
  }
});
