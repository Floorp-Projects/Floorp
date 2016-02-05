var cache = null;
var name = 'https_' + context;
var urlBase = 'https://example.com/tests/dom/cache/test/mochitest';
var url1 = urlBase + '/test_cache.js';
var url2 = urlBase + '/test_cache_add.js';

function addOpaque(cache, url) {
  return fetch(new Request(url, { mode: 'no-cors' })).then(function(response) {
    return cache.put(url, response);
  });
}

caches.open(name).then(function(c) {
  cache = c;
  return Promise.all([
    addOpaque(cache, url1),
    addOpaque(cache, url2)
  ]);
}).then(function() {
  return cache.delete(url1);
}).then(function(result) {
  ok(result, 'Cache entry should be deleted');
  return cache.delete(url2);
}).then(function(result) {
  ok(result, 'Cache entry should be deleted');
  cache = null;
  return caches.delete(name);
}).then(function(result) {
  ok(result, 'Cache should be deleted');
  testDone();
});
