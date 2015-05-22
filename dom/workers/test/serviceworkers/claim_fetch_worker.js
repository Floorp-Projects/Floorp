onfetch = function(e) {
  if (e.request.url.indexOf("service_worker_controlled") >= 0) {
    // pass through
    e.respondWith(fetch(e.request));
  } else {
    e.respondWith(new Response("Fetch was intercepted"));
  }
}

onmessage = function(e) {
  clients.claim();
}
