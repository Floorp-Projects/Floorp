self.addEventListener("fetch", function(event) {
  var resource = event.request.url.split('/').pop();
  event.waitUntil(
    clients.matchAll()
           .then(clients => {
             clients.forEach(client => {
               if (client.url.includes("plugins.html")) {
                 client.postMessage({destination: event.request.destination,
                                     resource: resource});
               }
             });
           })
  );
});
