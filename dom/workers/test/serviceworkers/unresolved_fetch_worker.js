onfetch = function(event) {
  if (!event.client) {
    dump("ERROR: event doesnt have a client");
  }

  event.client.postMessage("continue");

  // never resolve
  event.respondWith(new Promise(function(res, rej) {}));
}

onactivate = function(event) {
  event.waitUntil(clients.claim());
}
