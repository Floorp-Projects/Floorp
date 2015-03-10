var singleUrl = './test_cache_add.js';
var urlList = [
  './helloworld.txt',
  './foobar.txt',
  './test_cache.js'
];
var cache;
caches.open('adder').then(function(openCache) {
  cache = openCache;
  return cache.add('ftp://example.com/invalid');
}).catch(function (err) {
  is(err.name, 'NetworkError', 'add() should throw NetworkError for invalid scheme');
  return cache.addAll(['http://example.com/valid', 'ftp://example.com/invalid']);
}).catch(function (err) {
  is(err.name, 'NetworkError', 'addAll() should throw NetworkError for invalid scheme');
  var promiseList = urlList.map(function(url) {
    return cache.match(url);
  });
  promiseList.push(cache.match(singleUrl));
  return Promise.all(promiseList);
}).then(function(resultList) {
  is(urlList.length + 1, resultList.length, 'Expected number of results');
  resultList.every(function(result) {
    is(undefined, result, 'URLs should not already be in the cache');
  });
  return cache.add(singleUrl);
}).then(function(result) {
  is(undefined, result, 'Successful add() should resolve undefined');
  return cache.addAll(urlList);
}).then(function(result) {
  is(undefined, result, 'Successful addAll() should resolve undefined');
  var promiseList = urlList.map(function(url) {
    return cache.match(url);
  });
  promiseList.push(cache.match(singleUrl));
  return Promise.all(promiseList);
}).then(function(resultList) {
  is(urlList.length + 1, resultList.length, 'Expected number of results');
  resultList.every(function(result) {
    ok(!!result, 'Responses should now be in cache for each URL.');
  });
  return cache.matchAll();
}).then(function(resultList) {
  is(urlList.length + 1, resultList.length, 'Expected number of results');
  resultList.every(function(result) {
    ok(!!result, 'Responses should now be in cache for each URL.');
  });
  workerTestDone();
}).catch(function(err) {
  ok(false, 'Caught error: ' + err);
  workerTestDone();
});
