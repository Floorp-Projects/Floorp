// Similar test as "cacheir/set-has-object.js", except that we now perform
// duplicate lookups to ensure GVN works properly.

// Return a new set, possibly filling some dummy entries to enforce creating
// multiple hash buckets.
function createSet(values, n) {
  var xs = [...values];
  for (var i = 0; i < n; ++i) {
    xs.push({});
  }
  return new Set(xs);
}

function runTest(fn) {
  fn(0);
  fn(100);
}

function test_same_set(n) {
  var xs = [{}, {}];
  var ys = [{}, {}];
  var zs = [...xs, ...ys];
  var set = createSet(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set.has(z)) c++;
    if (set.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(test_same_set);

// Duplicate the above tests, but this time use a different set.

function test_different_set(n) {
  var xs = [{}, {}];
  var ys = [{}, {}];
  var zs = [...xs, ...ys];
  var set1 = createSet(xs, n);
  var set2 = createSet(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set1.has(z)) c++;
    if (set2.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(test_different_set);

// Test the alias information is correct.

function test_alias(n) {
  var xs = [{}, {}];
  var set = createSet([], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var x = xs[i & 1];

    set.add(x);
    if (set.has(x)) c++;

    set.delete(x);
    if (set.has(x)) c++;
  }
  assertEq(c, N);
}
runTest(test_alias);

// And finally test that we don't actually support GVN for objects, because the
// hash changes when moving an object.

function testRekey() {
  var set = new Set();
  var c = 0;
  var N = 100;
  for (var i = 0; i < N; ++i) {
    var k = {};
    set.add(k);

    if (set.has(k)) c++;

    minorgc();

    if (set.has(k)) c++;
  }

  assertEq(c, N * 2);
}
testRekey();
