// Similar test as "cacheir/set-has-bigint.js", except that we now perform
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

function testInlineDigitsSameSign_same_set(n) {
  var xs = [1n, 2n];
  var ys = [3n, 4n];
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
runTest(testInlineDigitsSameSign_same_set);

function testInlineDigitsDifferentSign_same_set(n) {
  var xs = [-1n, 2n];
  var ys = [1n, -2n];
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
runTest(testInlineDigitsDifferentSign_same_set);

function testHeapDigitsSameSign_same_set(n) {
  // Definitely uses heap digits.
  var heap = 2n ** 1000n;

  var xs = [heap + 1n, heap + 2n];
  var ys = [heap + 3n, heap + 4n];
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
runTest(testHeapDigitsSameSign_same_set);

function testHeapDigitsDifferentSign_same_set(n) {
  // Definitely uses heap digits.
  var heap = 2n ** 1000n;

  var xs = [-(heap + 1n), heap + 2n];
  var ys = [heap + 1n, -(heap + 2n)];
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
runTest(testHeapDigitsDifferentSign_same_set);

// Duplicate the above tests, but this time use a different set.

function testInlineDigitsSameSign_different_set(n) {
  var xs = [1n, 2n];
  var ys = [3n, 4n];
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
runTest(testInlineDigitsSameSign_different_set);

function testInlineDigitsDifferentSign_different_set(n) {
  var xs = [-1n, 2n];
  var ys = [1n, -2n];
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
runTest(testInlineDigitsDifferentSign_different_set);

function testHeapDigitsSameSign_different_set(n) {
  // Definitely uses heap digits.
  var heap = 2n ** 1000n;

  var xs = [heap + 1n, heap + 2n];
  var ys = [heap + 3n, heap + 4n];
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
runTest(testHeapDigitsSameSign_different_set);

function testHeapDigitsDifferentSign_different_set(n) {
  // Definitely uses heap digits.
  var heap = 2n ** 1000n;

  var xs = [-(heap + 1n), heap + 2n];
  var ys = [heap + 1n, -(heap + 2n)];
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
runTest(testHeapDigitsDifferentSign_different_set);

// Test the alias information is correct.

function test_alias(n) {
  var xs = [1n, 2n];
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
