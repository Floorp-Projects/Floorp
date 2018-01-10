self.skipWaiting();

addEventListener('fetch', event => {
  event.respondWith(
    fetch(event.request)
  );
});

addEventListener('message', function(event) {
  if (event.data === 'claim') {
    event.waitUntil(clients.claim());
  }
});
