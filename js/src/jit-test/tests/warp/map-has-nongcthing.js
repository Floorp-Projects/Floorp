// Similar test as "cacheir/map-has-nongcthing.js", except that we now perform
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

function testInt32_same_map(n) {
  var xs = [1, 2];
  var ys = [3, 4];
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
runTest(testInt32_same_map);

function testDouble_same_map(n) {
  var xs = [Math.PI, Infinity];
  var ys = [Math.E, -Infinity];
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
runTest(testDouble_same_map);

function testZero_same_map(n) {
  var xs = [0, -0];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map = createMap([0], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map.has(z)) c++;
    if (map.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testZero_same_map);

function testNaN_same_map(n) {
  var xs = [NaN, -NaN];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map = createMap([NaN], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map.has(z)) c++;
    if (map.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testNaN_same_map);

function testUndefinedAndNull_same_map(n) {
  var xs = [undefined, null];
  var ys = [1, -1];
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
runTest(testUndefinedAndNull_same_map);

function testBoolean_same_map(n) {
  var xs = [true, false];
  var ys = [1, -1];
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
runTest(testBoolean_same_map);

// Duplicate the above tests, but this time use a different map.

function testInt32_different_map(n) {
  var xs = [1, 2];
  var ys = [3, 4];
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
runTest(testInt32_different_map);

function testDouble_different_map(n) {
  var xs = [Math.PI, Infinity];
  var ys = [Math.E, -Infinity];
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
runTest(testDouble_different_map);

function testZero_different_map(n) {
  var xs = [0, -0];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map1 = createMap([0], n);
  var map2 = createMap([0], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map1.has(z)) c++;
    if (map2.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testZero_different_map);

function testNaN_different_map(n) {
  var xs = [NaN, -NaN];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map1 = createMap([NaN], n);
  var map2 = createMap([NaN], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map1.has(z)) c++;
    if (map2.has(z)) c++;
  }
  assertEq(c, N);
}
runTest(testNaN_different_map);

function testUndefinedAndNull_different_map(n) {
  var xs = [undefined, null];
  var ys = [1, -1];
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
runTest(testUndefinedAndNull_different_map);

function testBoolean_different_map(n) {
  var xs = [true, false];
  var ys = [1, -1];
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
runTest(testBoolean_different_map);

// Test the alias information is correct.

function testInt32_alias(n) {
  var xs = [1, 2];
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
runTest(testInt32_alias);

function testDouble_alias(n) {
  var xs = [Math.PI, Infinity];
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
runTest(testDouble_alias);

function testUndefinedAndNull_alias(n) {
  var xs = [undefined, null];
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
runTest(testUndefinedAndNull_alias);

function testBoolean_alias(n) {
  var xs = [true, false];
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
runTest(testBoolean_alias);
