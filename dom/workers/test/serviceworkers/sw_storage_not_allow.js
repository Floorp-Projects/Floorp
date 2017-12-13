let clientId;
addEventListener('fetch', function(event) {
  event.respondWith(async function() {
    if (event.request.url.includes('getClients')) {
      // Expected to fail since the storage access is not allowed.
      try {
        await self.clients.matchAll();
      } catch (e) {
        // expected failure
      }
    } else if (event.request.url.includes('getClient-stage1')) {
      let clients = await self.clients.matchAll();
      clientId = clients[0].id;
    } else if (event.request.url.includes('getClient-stage2')) {
      // Expected to fail since the storage access is not allowed.
      try {
        await self.clients.get(clientId);
      } catch(e) {
        // expected failure
      }
    }

    // Pass through the network request once our various Clients API
    // promises have completed.
    return await fetch(event.request);
  }());
});

addEventListener('message', function(event) {
  if (event.data === 'claim') {
    event.waitUntil(clients.claim());
  }
});

