// Similar test as "cacheir/set-has-nongcthing.js", except that we now perform
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

function testInt32_same_set(n) {
  var xs = [1, 2];
  var ys = [3, 4];
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
runTest(testInt32_same_set);

function testDouble_same_set(n) {
  var xs = [Math.PI, Infinity];
  var ys = [Math.E, -Infinity];
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
runTest(testDouble_same_set);

function testZero_same_set(n) {
  var xs = [0, -0];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var set = createSet([0], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set.has(z)) c++;
    if (set.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testZero_same_set);

function testNaN_same_set(n) {
  var xs = [NaN, -NaN];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var set = createSet([NaN], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set.has(z)) c++;
    if (set.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testNaN_same_set);

function testUndefinedAndNull_same_set(n) {
  var xs = [undefined, null];
  var ys = [1, -1];
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
runTest(testUndefinedAndNull_same_set);

function testBoolean_same_set(n) {
  var xs = [true, false];
  var ys = [1, -1];
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
runTest(testBoolean_same_set);

// Duplicate the above tests, but this time use a different set.

function testInt32_different_set(n) {
  var xs = [1, 2];
  var ys = [3, 4];
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
runTest(testInt32_different_set);

function testDouble_different_set(n) {
  var xs = [Math.PI, Infinity];
  var ys = [Math.E, -Infinity];
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
runTest(testDouble_different_set);

function testZero_different_set(n) {
  var xs = [0, -0];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var set1 = createSet([0], n);
  var set2 = createSet([0], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set1.has(z)) c++;
    if (set2.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testZero_different_set);

function testNaN_different_set(n) {
  var xs = [NaN, -NaN];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var set1 = createSet([NaN], n);
  var set2 = createSet([NaN], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set1.has(z)) c++;
    if (set2.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testNaN_different_set);

function testUndefinedAndNull_different_set(n) {
  var xs = [undefined, null];
  var ys = [1, -1];
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
runTest(testUndefinedAndNull_different_set);

function testBoolean_different_set(n) {
  var xs = [true, false];
  var ys = [1, -1];
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
runTest(testBoolean_different_set);

// Test the alias information is correct.

function testInt32_alias(n) {
  var xs = [1, 2];
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
runTest(testInt32_alias);

function testDouble_alias(n) {
  var xs = [Math.PI, Infinity];
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
runTest(testDouble_alias);

function testUndefinedAndNull_alias(n) {
  var xs = [undefined, null];
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
runTest(testUndefinedAndNull_alias);

function testBoolean_alias(n) {
  var xs = [true, false];
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
runTest(testBoolean_alias);
