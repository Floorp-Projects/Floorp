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
  } else if (event.request.url.indexOf("responsive.jpg") >= 0) {
    if (event.request.context == "imageset") {
      event.respondWith(fetch("realimg.jpg"));
    }
  } else if (event.request.url.indexOf("audio.ogg") >= 0) {
    if (event.request.context == "audio") {
      event.respondWith(fetch("realaudio.ogg"));
    }
  } else if (event.request.url.indexOf("video.ogg") >= 0) {
    // FIXME: Bug 1147668: This should be "video".
    if (event.request.context == "audio") {
      event.respondWith(fetch("realaudio.ogg"));
    }
  }
  // Fail any request that we don't know about.
  try {
    event.respondWith(Promise.reject(event.request.url));
  } catch(e) {
    // Eat up the possible InvalidStateError exception that we may get if some
    // code above has called respondWith too.
  }
});
