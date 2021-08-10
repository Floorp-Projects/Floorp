// Similar test as "cacheir/map-has-symbol.js", except that we now perform
// duplicate lookups to ensure GVN works properly.

// Return a new map, possibly filling some dummy entries to enforce creating
// multiple hash buckets.
function createMap(values, n) {
  var xs = [...values];
  for (var i = 0; i < n; ++i) {
    xs.push({});
  }
  return new Map(xs.map((x, i) => [x, i]));
}

function runTest(fn) {
  fn(0);
  fn(100);
}

function test_same_map(n) {
  var xs = [Symbol(), Symbol()];
  var ys = [Symbol(), Symbol()];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map.has(z)) c++;
    if (map.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(test_same_map);

// Duplicate the above tests, but this time use a different map.

function test_different_map(n) {
  var xs = [Symbol(), Symbol()];
  var ys = [Symbol(), Symbol()];
  var zs = [...xs, ...ys];
  var map1 = createMap(xs, n);
  var map2 = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map1.has(z)) c++;
    if (map2.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(test_different_map);

// Test the alias information is correct.

function test_alias(n) {
  var xs = [Symbol(), Symbol()];
  var map = createMap([], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var x = xs[i & 1];

    map.set(x, x);
    if (map.has(x)) c++;

    map.delete(x);
    if (map.has(x)) c++;
  }
  assertEq(c, N);
}
runTest(test_alias);
