self.addEventListener('fetch', function (event) {
  if (!event.request.url.endsWith('sw_clients/does_not_exist.html')) {
    return;
  }

  event.respondWith(new Response('', {
    status: 301,
    statusText: 'Moved Permanently',
    headers: {
      'Location': 'refresher_compressed.html'
    }
  }));
});
