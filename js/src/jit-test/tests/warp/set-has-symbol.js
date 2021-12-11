// Similar test as "cacheir/set-has-symbol.js", except that we now perform
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
  var xs = [Symbol(), Symbol()];
  var ys = [Symbol(), Symbol()];
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
  var xs = [Symbol(), Symbol()];
  var ys = [Symbol(), Symbol()];
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
  var xs = [Symbol(), Symbol()];
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
