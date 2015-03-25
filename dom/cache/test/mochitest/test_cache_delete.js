var name = "delete" + context;
var c;

function setupTest(reqs) {
  return new Promise(function(resolve, reject) {
    var cache;
    caches.open(name).then(function(c) {
        cache = c;
        return c.addAll(reqs);
      }).then(function() {
        resolve(cache);
      }).catch(function(err) {
        reject(err);
      });
  });
}

function testBasics() {
  var tests = [
    "//mochi.test:8888/?foo" + context,
    "//mochi.test:8888/?bar" + context,
  ];
  var cache;
  return setupTest(tests)
    .then(function(c) {
      cache = c;
      return cache.delete("//mochi.test:8888/?baz");
    }).then(function(deleted) {
      ok(!deleted, "Deleting a non-existing entry should fail");
      return cache.keys();
    }).then(function(keys) {
      is(keys.length, 2, "No entries from the cache should be deleted");
      return cache.delete(tests[0]);
    }).then(function(deleted) {
      ok(deleted, "Deleting an existing entry should succeed");
      return cache.keys();
    }).then(function(keys) {
      is(keys.length, 1, "Only one entry should exist now");
      ok(keys[0].url.indexOf(tests[1]) >= 0, "The correct entry must be deleted");
    });
}

function testFragment() {
  var tests = [
    "//mochi.test:8888/?foo" + context,
    "//mochi.test:8888/?bar" + context,
    "//mochi.test:8888/?baz" + context + "#fragment",
  ];
  var cache;
  return setupTest(tests)
    .then(function(c) {
      cache = c;
      return cache.delete(tests[0] + "#fragment");
    }).then(function(deleted) {
      ok(deleted, "Deleting an existing entry should succeed");
      return cache.keys();
    }).then(function(keys) {
      is(keys.length, 2, "Only one entry should exist now");
      ok(keys[0].url.indexOf(tests[1]) >= 0, "The correct entry must be deleted");
      ok(keys[1].url.indexOf(tests[2].replace("#fragment", "")) >= 0, "The correct entry must be deleted");
      // Now, delete a request that was added with a fragment
      return cache.delete("//mochi.test:8888/?baz" + context);
    }).then(function(deleted) {
      ok(deleted, "Deleting an existing entry should succeed");
      return cache.keys();
    }).then(function(keys) {
      is(keys.length, 1, "Only one entry should exist now");
      ok(keys[0].url.indexOf(tests[1]) >= 0, "3The correct entry must be deleted");
    });
}

function testInterleaved() {
  var tests = [
    "//mochi.test:8888/?foo" + context,
    "//mochi.test:8888/?bar" + context,
  ];
  var newURL = "//mochi.test:8888/?baz" + context;
  var cache;
  return setupTest(tests)
    .then(function(c) {
      cache = c;
      // Simultaneously add and delete a request
      return Promise.all([
        cache.delete(newURL),
        cache.add(newURL),
      ]);
    }).then(function(result) {
      ok(!result[1], "deletion should fail");
      return cache.keys();
    }).then(function(keys) {
      is(keys.length, 3, "Tree entries should still exist");
      ok(keys[0].url.indexOf(tests[0]) >= 0, "The correct entry must be deleted");
      ok(keys[1].url.indexOf(tests[1]) >= 0, "The correct entry must be deleted");
      ok(keys[2].url.indexOf(newURL) >= 0, "The new entry should be correctly inserted");
    });
}

// Make sure to clean up after each test step.
function step(testPromise) {
  return testPromise.then(function() {
    caches.delete(name);
  });
}

step(testBasics()).then(function() {
  return step(testFragment());
}).then(function() {
  return step(testInterleaved());
}).then(function() {
  testDone();
});
