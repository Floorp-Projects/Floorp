let clientId;
addEventListener('fetch', function(event) {
  if (event.request.url.includes('getClients')) {
    // Excepted to fail since the storage access is not allowed.
    self.clients.matchAll();
  } else if (event.request.url.includes('getClient-stage1')) {
    self.clients.matchAll().then(function(clients) {
      clientId = clients[0].id;
    });
  } else if (event.request.url.includes('getClient-stage2')) {
    // Excepted to fail since the storage access is not allowed.
    self.clients.get(clientId);
  }
});

addEventListener('message', function(event) {
  if (event.data === 'claim') {
    event.waitUntil(clients.claim());
  }
});

