var name = 'browserRefresherCache';

self.addEventListener('install', function(event) {
  event.waitUntil(
    Promise.all([caches.open(name),
                 fetch('./browser_cached_force_refresh.html')]).then(function(results) {
      var cache = results[0];
      var response = results[1];
      return cache.put('./browser_base_force_refresh.html', response);
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
