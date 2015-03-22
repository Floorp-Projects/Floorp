function arraysHaveSameContent(arr1, arr2) {
  if (arr1.length != arr2.length) {
    return false;
  }
  return arr1.every(function(value, index) {
    return arr2[index] === value;
  });
}

function testHas() {
  var name = "caches-has" + context;
  return caches.has(name).then(function(has) {
    ok(!has, name + " should not exist yet");
    return caches.open(name);
  }).then(function(c) {
    return caches.has(name);
  }).then(function(has) {
    ok(has, name + " should now exist");
    return caches.delete(name);
  }).then(function(deleted) {
    ok(deleted, "The deletion should finish successfully");
    return caches.has(name);
  }).then(function(has) {
    ok(!has, name + " should not exist any more");
  });
}

function testKeys() {
  var names = [
    // The names here are intentionally unsorted, to ensure the insertion order
    // and make sure we don't confuse it with an alphabetically sorted list.
    "caches-keys4" + context,
    "caches-keys0" + context,
    "caches-keys1" + context,
    "caches-keys3" + context,
  ];
  return caches.keys().then(function(keys) {
    is(keys.length, 0, "No keys should exist yet");
    return Promise.all(names.map(function(name) {
      return caches.open(name);
    }));
  }).then(function() {
    return caches.keys();
  }).then(function(keys) {
    ok(arraysHaveSameContent(keys, names), "Keys must match in insertion order");
    return Promise.all(names.map(function(name) {
      return caches.delete(name);
    }));
  }).then(function(deleted) {
    ok(arraysHaveSameContent(deleted, [true, true, true, true]), "All deletions must succeed");
    return caches.keys();
  }).then(function(keys) {
    is(keys.length, 0, "No keys should exist any more");
  });
}

function testMatchAcrossCaches() {
  var tests = [
    // The names here are intentionally unsorted, to ensure the insertion order
    // and make sure we don't confuse it with an alphabetically sorted list.
    {
      name: "caches-xmatch5" + context,
      request: "//mochi.test:8888/?5" + context,
    },
    {
      name: "caches-xmatch2" + context,
      request: "//mochi.test:8888/tests/dom/cache/test/mochitest/test_caches.js?2" + context,
    },
    {
      name: "caches-xmatch4" + context,
      request: "//mochi.test:8888/?4" + context,
    },
  ];
  return Promise.all(tests.map(function(test) {
    return caches.open(test.name).then(function(c) {
      return c.add(test.request);
    });
  })).then(function() {
    return caches.match("//mochi.test:8888/?5" + context, {ignoreSearch: true});
  }).then(function(match) {
    ok(match.url.indexOf("?5") > 0, "Match should come from the first cache");
    return caches.delete("caches-xmatch2" + context); // This should not change anything!
  }).then(function(deleted) {
    ok(deleted, "Deletion should finish successfully");
    return caches.match("//mochi.test:8888/?" + context, {ignoreSearch: true});
  }).then(function(match) {
    ok(match.url.indexOf("?5") > 0, "Match should still come from the first cache");
    return caches.delete("caches-xmatch5" + context); // This should eliminate the first match!
  }).then(function(deleted) {
    ok(deleted, "Deletion should finish successfully");
    return caches.match("//mochi.test:8888/?" + context, {ignoreSearch: true});
  }).then(function(match) {
    ok(match.url.indexOf("?4") > 0, "Match should come from the third cache");
    return caches.delete("caches-xmatch4" + context); // Game over!
  }).then(function(deleted) {
    ok(deleted, "Deletion should finish successfully");
    return caches.match("//mochi.test:8888/?" + context, {ignoreSearch: true});
  }).then(function(match) {
    is(typeof match, "undefined", "No matches should be found");
  });
}

function testDelete() {
  return caches.delete("delete" + context).then(function(deleted) {
    ok(!deleted, "Attempting to delete a non-existing cache should fail");
    return caches.open("delete" + context);
  }).then(function() {
    return caches.delete("delete" + context);
  }).then(function(deleted) {
    ok(deleted, "Delete should now succeed");
  });
}

testHas().then(function() {
  return testKeys();
}).then(function() {
  return testMatchAcrossCaches();
}).then(function() {
  return testDelete();
}).then(function() {
  testDone();
});
