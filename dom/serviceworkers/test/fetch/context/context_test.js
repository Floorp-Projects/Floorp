self.addEventListener("fetch", function(event) {
  if (event.request.url.includes("index.html") ||
      event.request.url.includes("register.html") ||
      event.request.url.includes("unregister.html") ||
      event.request.url.includes("ping.html") ||
      event.request.url.includes("xml.xml") ||
      event.request.url.includes("csp-violate.sjs")) {
    // Handle pass-through requests
    event.respondWith(fetch(event.request));
  } else if (event.request.url.includes("fetch.txt")) {
    var body = event.request.context == "fetch" ?
               "so fetch" : "so unfetch";
    event.respondWith(new Response(body));
  } else if (event.request.url.includes("img.jpg")) {
    if (event.request.context == "image") {
      event.respondWith(fetch("realimg.jpg"));
    }
  } else if (event.request.url.includes("responsive.jpg")) {
    if (event.request.context == "imageset") {
      event.respondWith(fetch("realimg.jpg"));
    }
  } else if (event.request.url.includes("audio.ogg")) {
    if (event.request.context == "audio") {
      event.respondWith(fetch("realaudio.ogg"));
    }
  } else if (event.request.url.includes("video.ogg")) {
    if (event.request.context == "video") {
      event.respondWith(fetch("realaudio.ogg"));
    }
  } else if (event.request.url.includes("beacon.sjs")) {
    if (!event.request.url.includes("queryContext")) {
      event.respondWith(fetch("beacon.sjs?" + event.request.context));
    } else {
      event.respondWith(fetch(event.request));
    }
  } else if (event.request.url.includes("csp-report.sjs")) {
    respondToServiceWorker(event, "csp-report");
  } else if (event.request.url.includes("embed")) {
    respondToServiceWorker(event, "embed");
  } else if (event.request.url.includes("object")) {
    respondToServiceWorker(event, "object");
  } else if (event.request.url.includes("font")) {
    respondToServiceWorker(event, "font");
  } else if (event.request.url.includes("iframe")) {
    if (event.request.context == "iframe") {
      event.respondWith(fetch("context_test.js"));
    }
  } else if (event.request.url.includes("frame")) {
    if (event.request.context == "frame") {
      event.respondWith(fetch("context_test.js"));
    }
  } else if (event.request.url.includes("newwindow")) {
    respondToServiceWorker(event, "newwindow");
  } else if (event.request.url.includes("ping")) {
    respondToServiceWorker(event, "ping");
  } else if (event.request.url.includes("plugin")) {
    respondToServiceWorker(event, "plugin");
  } else if (event.request.url.includes("script.js")) {
    if (event.request.context == "script") {
      event.respondWith(new Response(""));
    }
  } else if (event.request.url.includes("style.css")) {
    respondToServiceWorker(event, "style");
  } else if (event.request.url.includes("track")) {
    respondToServiceWorker(event, "track");
  } else if (event.request.url.includes("xhr")) {
    if (event.request.context == "xmlhttprequest") {
      event.respondWith(new Response(""));
    }
  } else if (event.request.url.includes("xslt")) {
    respondToServiceWorker(event, "xslt");
   } else if (event.request.url.includes("myworker")) {
     if (event.request.context == "worker") {
       event.respondWith(fetch("worker.js"));
     }
   } else if (event.request.url.includes("myparentworker")) {
     if (event.request.context == "worker") {
       event.respondWith(fetch("parentworker.js"));
     }
   } else if (event.request.url.includes("mysharedworker")) {
     if (event.request.context == "sharedworker") {
       event.respondWith(fetch("sharedworker.js"));
     }
   } else if (event.request.url.includes("myparentsharedworker")) {
     if (event.request.context == "sharedworker") {
       event.respondWith(fetch("parentsharedworker.js"));
     }
  } else if (event.request.url.includes("cache")) {
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
