var name = 'refresherCache';

self.addEventListener('install', function(event) {
  event.waitUntil(
    Promise.all([caches.open(name),
                 fetch('./sw_clients/refresher_cached.html'),
                 fetch('./sw_clients/refresher_cached_compressed.html')]).then(function(results) {
      var cache = results[0];
      var response = results[1];
      var compressed = results[2];
      return Promise.all([cache.put('./sw_clients/refresher.html', response),
                          cache.put('./sw_clients/refresher_compressed.html', compressed)]);
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
