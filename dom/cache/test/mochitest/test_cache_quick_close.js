caches.open('closer').then(function (cache) {
  cache.add('./test_cache.js').then(function() { }, function() { });
});
self.close();
ok(true, 'Should not crash on close()');
workerTestDone();
