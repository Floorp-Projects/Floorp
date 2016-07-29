addEventListener('install', function(evt) {
  evt.waitUntil(self.skipWaiting());
});

addEventListener('activate', function(evt) {
  // We claim the current clients in order to ensure that we have an
  // active client when we call unregister in the fetch handler.  Otherwise
  // the unregister() can kill the current worker before returning a
  // response.
  evt.waitUntil(clients.claim());
});

addEventListener('fetch', function(evt) {
  // This worker may live long enough to receive a fetch event from the next
  // test.  Just pass such requests through to the network.
  if (evt.request.url.indexOf('fake_download') === -1) {
    return;
  }

  // We should only get a single download fetch event. Automatically unregister.
  evt.respondWith(registration.unregister().then(function() {
    return new Response('service worker generated download', {
      headers: {
        'Content-Disposition': 'attachment; filename="fake_download.bin"',
        // fake encoding header that should have no effect
        'Content-Encoding': 'gzip',
      }
    });
  }));
});
