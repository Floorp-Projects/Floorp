var singleUrl = './test_cache_add.js';
var urlList = [
  './helloworld.txt',
  './foobar.txt',
  './test_cache.js'
];
var cache;
var name = "adder" + context;
caches.open(name).then(function(openCache) {
  cache = openCache;
  return cache.add('ftp://example.com/invalid' + context);
}).catch(function (err) {
  is(err.name, 'TypeError', 'add() should throw TypeError for invalid scheme');
  return cache.addAll(['http://example.com/valid' + context, 'ftp://example.com/invalid' + context]);
}).catch(function (err) {
  is(err.name, 'TypeError', 'addAll() should throw TypeError for invalid scheme');
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
  return caches.delete(name);
}).then(function() {
  testDone();
}).catch(function(err) {
  ok(false, 'Caught error: ' + err);
  testDone();
});
