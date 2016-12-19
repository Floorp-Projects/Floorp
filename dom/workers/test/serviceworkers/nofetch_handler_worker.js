function handleFetch(event) {
  event.respondWith(new Response('intercepted'));
}

self.oninstall = function(event) {
      addEventListener('fetch', handleFetch);
      self.onfetch = handleFetch;
}
