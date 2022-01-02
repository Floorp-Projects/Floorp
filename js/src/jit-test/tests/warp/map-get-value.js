// Similar test as "cacheir/map-get-value.js", except that we now perform
// duplicate lookups to ensure GVN works properly.

// Return a new map, possibly filling some dummy entries to enforce creating
// multiple hash buckets.
function createMap(values, n) {
  var xs = [...values];
  for (var i = 0; i < n; ++i) {
    xs.push({});
  }
  return new Map(xs.map((x, i) => [x, i + 1]));
}

function runTest(fn) {
  fn(0);
  fn(100);
}

function testPolymorphic_same_map(n) {
  var xs = [10, 10.5, "test", Symbol("?"), 123n, -123n, {}, []];
  var ys = [-0, NaN, "bad", Symbol("!"), 42n, -99n, {}, []];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 128;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 15];
    var v = map.get(z);
    if (v !== undefined) c += v;
    var w = map.get(z);
    if (w !== undefined) c += w;
  }
  assertEq(c, (8 * 9) / 2 * 8 * 2);
}
runTest(testPolymorphic_same_map);

// Duplicate the above tests, but this time use a different map.

function testPolymorphic_different_map(n) {
  var xs = [10, 10.5, "test", Symbol("?"), 123n, -123n, {}, []];
  var ys = [-0, NaN, "bad", Symbol("!"), 42n, -99n, {}, []];
  var zs = [...xs, ...ys];
  var map1 = createMap(xs, n);
  var map2 = createMap(xs, n);

  var N = 128;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 15];
    var v = map1.get(z);
    if (v !== undefined) c += v;
    var w = map2.get(z);
    if (w !== undefined) c += w;
  }
  assertEq(c, (8 * 9) / 2 * 8 * 2);
}
runTest(testPolymorphic_different_map);

// Test the alias information is correct.

function testPolymorphic_alias(n) {
  var xs = [10, 10.5, "test", Symbol("?"), 123n, -123n, {}, []];
  var map = createMap([], n);

  var N = 128;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var x = xs[i & 1];

    map.set(x, 1);
    var v = map.get(x);

    map.delete(x);
    var w = map.get(x);

    c += v;
    assertEq(w, undefined);
  }
  assertEq(c, N);
}
runTest(testPolymorphic_alias);

// And finally test that we don't actually support GVN for values, because the
// hash changes when moving a value which holds an object.

function testRekey() {
  var map = new Map();
  var c = 0;
  var N = 100;
  for (var i = 0; i < N; ++i) {
    var k = (i & 1) ? {} : null;
    map.set(k, 1);

    c += map.get(k);

    minorgc();

    c += map.get(k);
  }

  assertEq(c, N * 2);
}
testRekey();
