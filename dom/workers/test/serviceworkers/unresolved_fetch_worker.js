var keepPromiseAlive;
onfetch = function(event) {
  event.waitUntil(
    clients.matchAll()
           .then(clients => {
             clients.forEach(client => {
               client.postMessage("continue");
             });
           })
  );

  // Never resolve, and keep it alive on our global so it can't get GC'ed and
  // make this test weird and intermittent.
  event.respondWith((keepPromiseAlive = new Promise(function(res, rej) {})));
}

onactivate = function(event) {
  event.waitUntil(clients.claim());
}
