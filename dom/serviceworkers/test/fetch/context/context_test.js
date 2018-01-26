self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0 ||
      event.request.url.indexOf("register.html") >= 0 ||
      event.request.url.indexOf("unregister.html") >= 0 ||
      event.request.url.indexOf("ping.html") >= 0 ||
      event.request.url.indexOf("xml.xml") >= 0 ||
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
    if (event.request.context == "video") {
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
    if (event.request.context == "frame") {
      event.respondWith(fetch("context_test.js"));
    }
  } else if (event.request.url.indexOf("newwindow") >= 0) {
    respondToServiceWorker(event, "newwindow");
  } else if (event.request.url.indexOf("ping") >= 0) {
    respondToServiceWorker(event, "ping");
  } else if (event.request.url.indexOf("plugin") >= 0) {
    respondToServiceWorker(event, "plugin");
  } else if (event.request.url.indexOf("script.js") >= 0) {
    if (event.request.context == "script") {
      event.respondWith(new Response(""));
    }
  } else if (event.request.url.indexOf("style.css") >= 0) {
    respondToServiceWorker(event, "style");
  } else if (event.request.url.indexOf("track") >= 0) {
    respondToServiceWorker(event, "track");
  } else if (event.request.url.indexOf("xhr") >= 0) {
    if (event.request.context == "xmlhttprequest") {
      event.respondWith(new Response(""));
    }
  } else if (event.request.url.indexOf("xslt") >= 0) {
    respondToServiceWorker(event, "xslt");
   } else if (event.request.url.indexOf("myworker") >= 0) {
     if (event.request.context == "worker") {
       event.respondWith(fetch("worker.js"));
     }
   } else if (event.request.url.indexOf("myparentworker") >= 0) {
     if (event.request.context == "worker") {
       event.respondWith(fetch("parentworker.js"));
     }
   } else if (event.request.url.indexOf("mysharedworker") >= 0) {
     if (event.request.context == "sharedworker") {
       event.respondWith(fetch("sharedworker.js"));
     }
   } else if (event.request.url.indexOf("myparentsharedworker") >= 0) {
     if (event.request.context == "sharedworker") {
       event.respondWith(fetch("parentsharedworker.js"));
     }
  } else if (event.request.url.indexOf("cache") >= 0) {
    var cache;
    var origContext = event.request.context;
    event.respondWith(caches.open("cache")
      .then(function(c) {
        cache = c;
        // Store the Request in the cache.
        return cache.put(event.request, new Response("fake"));
      }).then(function() {
        // Read it back.
        return cache.keys(event.request);
      }).then(function(res) {
        var req = res[0];
        // Check to see if the context remained the same.
        var success = req.context === origContext;
        return clients.matchAll()
               .then(function(clients) {
                 // Report it back to the main page.
                 clients.forEach(function(c) {
                   c.postMessage({data: "cache", success: success});
                 });
      })}).then(function() {
        // Cleanup.
        return caches.delete("cache");
      }).then(function() {
        return new Response("ack");
      }));
  }
  // Fail any request that we don't know about.
  try {
    event.respondWith(Promise.reject(event.request.url));
    dump("Fetch event received invalid context value " + event.request.context +
         " for " + event.request.url + "\n");
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
