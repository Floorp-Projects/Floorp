function handleFetch(event) {
  event.respondWith(new Response('intercepted'));
}

self.oninstall = function(event) {
      addEventListener('fetch', handleFetch);
      self.onfetch = handleFetch;
}

// Bug 1325101. Make sure adding event listeners for other events
// doesn't set the fetch flag.
addEventListener('push', function() {})
addEventListener('message', function() {})
addEventListener('non-sw-event', function() {})
