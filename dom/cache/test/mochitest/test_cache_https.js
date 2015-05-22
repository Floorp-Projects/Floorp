var cache = null;
var name = 'https_' + context;
var urlBase = 'https://example.com/tests/dom/cache/test/mochitest';
var url1 = urlBase + '/test_cache.js';
var url2 = urlBase + '/test_cache_add.js';

caches.open(name).then(function(c) {
  cache = c;
  return cache.addAll([new Request(url1, { mode: 'no-cors' }),
                       new Request(url2, { mode: 'no-cors' })]);
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
