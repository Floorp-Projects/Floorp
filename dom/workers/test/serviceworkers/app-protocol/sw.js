self.addEventListener('fetch', (event) => {
  if (event.request.url.indexOf('foo.txt') >= 0) {
    event.respondWith(new Response('swresponse', {
      headers: {'Content-Type': 'text/plain'}
    }));
  }

  if (event.request.url.indexOf('test_doc_load_interception.js') >=0 ) {
    var scriptContent = 'alert("OK: Script modified by service worker")';
    event.respondWith(new Response(scriptContent, {
      headers: {'Content-Type': 'application/javascript'}
    }));
  }
});
