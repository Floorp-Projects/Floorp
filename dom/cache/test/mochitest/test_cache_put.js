var url = 'test_cache.js';
var cache;
var fetchResponse;
Promise.all([fetch(url),
             caches.open('putter' + context)]).then(function(results) {
  fetchResponse = results[0];
  cache = results[1];
  return cache.put(url, fetchResponse.clone());
}).then(function(result) {
  is(undefined, result, 'Successful put() should resolve undefined');
  return cache.match(url);
}).then(function(response) {
  ok(response, 'match() should find resppnse that was previously put()');
  ok(response.url.endsWith(url), 'matched response should match original url');
  return Promise.all([fetchResponse.text(),
                      response.text()]);
}).then(function(results) {
  // suppress large assert spam unless it's relevent
  if (results[0] !== results[1]) {
    is(results[0], results[1], 'stored response body should match original');
  }

  // Now, try to overwrite the request with a different response object.
  return cache.put(url, new Response("overwritten"));
}).then(function() {
  return cache.matchAll(url);
}).then(function(result) {
  is(result.length, 1, "Only one entry should exist");
  return result[0].text();
}).then(function(body) {
  is(body, "overwritten", "The cache entry should be successfully overwritten");

  // Now, try to write a URL with a fragment
  return cache.put(url + "#fragment", new Response("more overwritten"));
}).then(function() {
  return cache.matchAll(url + "#differentFragment");
}).then(function(result) {
  is(result.length, 1, "Only one entry should exist");
  return result[0].text();
}).then(function(body) {
  is(body, "more overwritten", "The cache entry should be successfully overwritten");

  // TODO: Verify that trying to store a response with an error raises a TypeError
  // when bug 1147178 is fixed.

  return caches.delete('putter' + context);
}).then(function(deleted) {
  ok(deleted, "The cache should be deleted successfully");
  testDone();
});
