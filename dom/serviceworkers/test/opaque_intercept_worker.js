var name = 'opaqueInterceptCache';

// Cross origin request to ensure that an opaque response is used
var prefix = 'http://example.com/tests/dom/workers/test/serviceworkers/'

self.addEventListener('install', function(event) {
  var request = new Request(prefix + 'notify_loaded.js', { mode: 'no-cors' });
  event.waitUntil(
    Promise.all([caches.open(name), fetch(request)]).then(function(results) {
      var cache = results[0];
      var response = results[1];
      return cache.put('./sw_clients/does_not_exist.js', response);
    })
  );
});

self.addEventListener('fetch', function (event) {
  event.respondWith(
    caches.open(name).then(function(cache) {
      return cache.match(event.request);
    }).then(function(response) {
      return response || fetch(event.request);
    })
  );
});
