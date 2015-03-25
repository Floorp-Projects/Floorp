self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0 ||
      event.request.url.indexOf("register.html") >= 0 ||
      event.request.url.indexOf("unregister.html") >= 0) {
    // Handle pass-through requests
    event.respondWith(fetch(event.request));
  } else if (event.request.url.indexOf("fetch.txt") >= 0) {
    var body = event.request.context == "fetch" ?
               "so fetch" : "so unfetch";
    event.respondWith(new Response(body));
  } else if (event.request.url.indexOf("img.jpg") >= 0) {
    if (event.request.context == "image") {
      event.respondWith(fetch("realimg.jpg"));
    }
  } else {
    // Fail any request that we don't know about.
    event.respondWith(Promise.reject());
  }
});
