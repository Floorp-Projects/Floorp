self.addEventListener('fetch', (event) => {
  if (event.request.url.indexOf('foo.txt') >= 0) {
    var body = 'swresponse';
    event.respondWith(new Response(body, {
      headers: {'Content-Type': 'text/plain'}
    }));
  }
});
