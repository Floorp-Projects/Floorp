ok(!!caches, 'caches object should be available on global');
caches.open('snafu').then(function(openCache) {
  ok(openCache instanceof Cache, 'cache object should be resolved from caches.open');
  return caches.has('snafu');
}).then(function(hasResult) {
  ok(hasResult, 'caches.has() should resolve true');
  return caches.keys();
}).then(function(keys) {
  ok(!!keys, 'caches.keys() should resolve to a truthy value');
  is(1, keys.length, 'caches.keys() should resolve to an array of length 1');
  is(0, keys.indexOf('snafu'), 'caches.keys() should resolve to an array containing key');
  return caches.delete('snafu');
}).then(function(deleteResult) {
  ok(deleteResult, 'caches.delete() should resolve true');
  return caches.has('snafu');
}).then(function(hasMissingCache) {
  ok(!hasMissingCache, 'missing key should return false from has');
}).then(function() {
  return caches.open('snafu');
}).then(function(snafu) {
  return snafu.keys();
}).then(function(empty) {
  is(0, empty.length, 'cache.keys() should resolve to an array of length 0');
}).then(function() {
  return caches.open('snafu');
}).then(function(snafu) {
  var req = './cachekey';
  var res = new Response("Hello world");
  return snafu.put('ftp://invalid', res).catch(function (err) {
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
  workerTestDone();
})
