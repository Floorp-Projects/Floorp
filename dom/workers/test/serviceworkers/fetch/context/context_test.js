self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0 ||
      event.request.url.indexOf("register.html") >= 0 ||
      event.request.url.indexOf("unregister.html") >= 0 ||
      event.request.url.indexOf("csp-violate.sjs") >= 0) {
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
  } else if (event.request.url.indexOf("beacon.sjs") >= 0) {
    if (event.request.url.indexOf("queryContext") == -1) {
      event.respondWith(fetch("beacon.sjs?" + event.request.context));
    } else {
      event.respondWith(fetch(event.request));
    }
  } else if (event.request.url.indexOf("csp-report.sjs") >= 0) {
    respondToServiceWorker(event, "csp-report");
  } else if (event.request.url.indexOf("embed") >= 0) {
    respondToServiceWorker(event, "embed");
  } else if (event.request.url.indexOf("object") >= 0) {
    respondToServiceWorker(event, "object");
  } else if (event.request.url.indexOf("font") >= 0) {
    respondToServiceWorker(event, "font");
  } else if (event.request.url.indexOf("iframe") >= 0) {
    if (event.request.context == "iframe") {
      event.respondWith(fetch("context_test.js"));
    }
  } else if (event.request.url.indexOf("frame") >= 0) {
    // FIXME: Bug 1148044: This should be "frame".
    if (event.request.context == "iframe") {
      event.respondWith(fetch("context_test.js"));
    }
  } else if (event.request.url.indexOf("newwindow") >= 0) {
    respondToServiceWorker(event, "newwindow");
  }
  // Fail any request that we don't know about.
  try {
    event.respondWith(Promise.reject(event.request.url));
  } catch(e) {
    // Eat up the possible InvalidStateError exception that we may get if some
    // code above has called respondWith too.
  }
});

function respondToServiceWorker(event, data) {
  event.respondWith(clients.matchAll()
                    .then(function(clients) {
                      clients.forEach(function(c) {
                        c.postMessage({data: data, context: event.request.context});
                      });
                      return new Response("ack");
                    }));
}
