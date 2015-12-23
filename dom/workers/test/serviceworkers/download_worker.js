addEventListener('fetch', function(evt) {
  // We should only get a single fetch event.  Automatically unregister.
  evt.respondWith(registration.unregister().then(function() {
    return new Response('service worker generated download', {
      headers: {
        'Content-Disposition': 'attachment; filename="fake_download.bin"'
      }
    });
  }));
});
