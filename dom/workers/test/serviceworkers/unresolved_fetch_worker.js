onfetch = function(event) {
  event.waitUntil(
    clients.matchAll()
           .then(clients => {
             clients.forEach(client => {
               client.postMessage("continue");
             });
           })
  );

  // never resolve
  event.respondWith(new Promise(function(res, rej) {}));
}

onactivate = function(event) {
  event.waitUntil(clients.claim());
}
