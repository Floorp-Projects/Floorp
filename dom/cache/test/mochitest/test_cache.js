var c = null
var request = "http://example.com/hmm?q=foobar" + context;
var response = new Response("This is some Response!");
var name = 'snafu' + context;
var foobar = 'foobar' + context;

ok(!!caches, 'caches object should be available on global');
caches.open(name).then(function(openCache) {
  ok(openCache instanceof Cache, 'cache object should be resolved from caches.open');
  return caches.has(name);
}).then(function(hasResult) {
  ok(hasResult, 'caches.has() should resolve true');
  return caches.keys();
}).then(function(keys) {
  ok(!!keys, 'caches.keys() should resolve to a truthy value');
  ok(keys.length >= 1, 'caches.keys() should resolve to an array of length at least 1');
  ok(keys.indexOf(name) >= 0, 'caches.keys() should resolve to an array containing key');
  return caches.delete(name);
}).then(function(deleteResult) {
  ok(deleteResult, 'caches.delete() should resolve true');
  return caches.has(name);
}).then(function(hasMissingCache) {
  ok(!hasMissingCache, 'missing key should return false from has');
}).then(function() {
  return caches.open(name);
}).then(function(snafu) {
  return snafu.keys();
}).then(function(empty) {
  is(0, empty.length, 'cache.keys() should resolve to an array of length 0');
}).then(function() {
  return caches.open(name);
}).then(function(snafu) {
  var req = './cachekey';
  var res = new Response("Hello world");
  return snafu.put('ftp://invalid', res).then(function() {
    ok(false, 'This should fail');
  }).catch(function (err) {
    is(err.name, 'TypeError', 'put() should throw TypeError for invalid scheme');
    return snafu.put(req, res);
  }).then(function(v) {
    return snafu;
  });
}).then(function(snafu) {
  return Promise.all([snafu, snafu.keys()]);
}).then(function(args) {
  var snafu = args[0];
  var keys = args[1];
  is(1, keys.length, 'cache.keys() should resolve to an array of length 1');
  ok(keys[0] instanceof Request, 'key should be a Request');
  ok(keys[0].url.match(/cachekey$/), 'Request URL should match original');
  return Promise.all([snafu, snafu.match(keys[0]), snafu.match('ftp://invalid')]);
}).then(function(args) {
  var snafu = args[0];
  var response = args[1];
  ok(response instanceof Response, 'value should be a Response');
  is(response.status, 200, 'Response status should be 200');
  is(undefined, args[2], 'Match with invalid scheme should resolve undefined');
  return Promise.all([snafu, snafu.put('./cachekey2', response)]);
}).then(function(args) {
  var snafu = args[0]
  return snafu.match('./cachekey2');
}).then(function(response) {
  return response.text().then(function(v) {
    is(v, "Hello world", "Response body should match original");
  });
}).then(function() {
  // FIXME(nsm): Can't use a Request object for now since the operations
  // consume it's 'body'. See
  // https://github.com/slightlyoff/ServiceWorker/issues/510.
  return caches.open(foobar);
}).then(function(openCache) {
  c = openCache;
  return c.put(request, response);
}).then(function(putResponse) {
  is(putResponse, undefined, 'The promise should resolve to undefined');
  return c.keys(request);
}).then(function(keys) {
  ok(keys, 'Valid keys object expected');
  is(keys.length, 1, 'Only one key is expected');
  return c.keys();
}).then(function(keys) {
  ok(keys, 'Valid keys object expected');
  is(keys.length, 1, 'Only one key is expected');
  return c.matchAll(request);
}).then(function(matchAllResponses) {
  ok(matchAllResponses, 'matchAll should succeed');
  is(matchAllResponses.length, 1, 'Only one match is expected');
  return c.match(request);
}).then(function(matchResponse) {
  ok(matchResponse, 'match should succeed');
  return caches.match(request);
}).then(function(storageMatchResponse) {
  ok(storageMatchResponse, 'storage match should succeed');
  return caches.match(request, {cacheName:foobar});
}).then(function(storageMatchResponse) {
  ok(storageMatchResponse, 'storage match with cacheName should succeed');
  var request2 = new Request("http://example.com/hmm?q=snafu" + context);
  return c.match(request2, {ignoreSearch:true});
}).then(function(match2Response) {
  ok(match2Response, 'match should succeed');
  return c.delete(request);
}).then(function(deleteResult) {
  ok(deleteResult, 'delete should succeed');
  return c.keys();
}).then(function(keys) {
  ok(keys, 'Valid keys object expected');
  is(keys.length, 0, 'Zero keys is expected');
  return c.matchAll(request);
}).then(function(matchAll2Responses) {
  ok(matchAll2Responses, 'matchAll should succeed');
  is(matchAll2Responses.length, 0, 'Zero matches is expected');
  return caches.has(foobar);
}).then(function(hasResult) {
  ok(hasResult, 'has should succeed');
  return caches.keys();
}).then(function(keys) {
  ok(keys, 'Valid keys object expected');
  ok(keys.length >= 2, 'At least two keys are expected');
  ok(keys.indexOf(name) >= 0, 'snafu should exist');
  ok(keys.indexOf(foobar) >= keys.indexOf(name), 'foobar should come after it');
  return caches.delete(foobar);
}).then(function(deleteResult) {
  ok(deleteResult, 'delete should succeed');
  return caches.has(foobar);
}).then(function(hasMissingCache) {
  ok(!hasMissingCache, 'has should have a result');
  return caches.delete(name);
}).then(function(deleteResult) {
  ok(deleteResult, 'delete should succeed');
  testDone();
})
