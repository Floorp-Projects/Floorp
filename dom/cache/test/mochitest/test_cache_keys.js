var name = "keys" + context;
var c;

var tests = [
  "//mochi.test:8888/?page" + context,
  "//mochi.test:8888/?another" + context,
];

caches.open(name).then(function(cache) {
  c = cache;
  return c.addAll(tests);
}).then(function() {
  // Add another cache entry using Cache.add
  var another = "//mochi.test:8888/?yetanother" + context;
  tests.push(another);
  return c.add(another);
}).then(function() {
  // Add another cache entry with URL fragment using Cache.add
  var anotherWithFragment = "//mochi.test:8888/?fragment" + context + "#fragment";
  tests.push(anotherWithFragment);
  return c.add(anotherWithFragment);
}).then(function() {
  return c.keys();
}).then(function(keys) {
  is(keys.length, tests.length, "Same number of elements");
  // Verify both the insertion order of the requests and their validity.
  keys.forEach(function(r, i) {
    ok(r instanceof Request, "Valid request object");
    ok(r.url.indexOf(tests[i]) >= 0, "Valid URL");
  });
  // Try searching for just one request
  return c.keys(tests[1]);
}).then(function(keys) {
  is(keys.length, 1, "One match should be found");
  ok(keys[0].url.indexOf(tests[1]) >= 0, "Valid URL");
  // Try to see if ignoreSearch works as expected.
  return c.keys(new Request("//mochi.test:8888/?foo"), {ignoreSearch: true});
}).then(function(keys) {
  is(keys.length, tests.length, "Same number of elements");
  keys.forEach(function(r, i) {
    ok(r instanceof Request, "Valid request object");
    ok(r.url.indexOf(tests[i]) >= 0, "Valid URL");
  });
  // Try to see if ignoreMethod works as expected
  return Promise.all(
    ["POST", "PUT", "DELETE", "OPTIONS"]
      .map(function(method) {
        var req = new Request(tests[2], {method: method});
        return c.keys(req)
          .then(function(keys) {
            is(keys.length, 0, "No request should be matched without ignoreMethod");
            return c.keys(req, {ignoreMethod: true});
          }).then(function(keys) {
            is(keys.length, 1, "One match should be found");
            ok(keys[0].url.indexOf(tests[2]) >= 0, "Valid URL");
          });
      })
  );
}).then(function() {
  // But HEAD should be allowed even without ignoreMethod
  return c.keys(new Request(tests[0], {method: "HEAD"}));
}).then(function(keys) {
  is(keys.length, 1, "One match should be found");
  ok(keys[0].url.indexOf(tests[0]) >= 0, "Valid URL");
  // Make sure cacheName is ignored.
  return c.keys(tests[0], {cacheName: "non-existing-cache"});
}).then(function(keys) {
  is(keys.length, 1, "One match should be found");
  ok(keys[0].url.indexOf(tests[0]) >= 0, "Valid URL");
  return caches.delete(name);
}).then(function(deleted) {
  ok(deleted, "The cache should be successfully deleted");
  testDone();
});
