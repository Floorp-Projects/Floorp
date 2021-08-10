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

function testInt32(n) {
  var xs = [1, 2];
  var ys = [3, 4];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2 + N / 4);
}
runTest(testInt32);

function testDouble(n) {
  var xs = [Math.PI, Infinity];
  var ys = [Math.E, -Infinity];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2 + N / 4);
}
runTest(testDouble);

function testZero(n) {
  var xs = [0, -0];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map = createMap([0], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2);
}
runTest(testZero);

function testNaN(n) {
  var xs = [NaN, -NaN];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map = createMap([NaN], n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2);
}
runTest(testNaN);

function testUndefinedAndNull(n) {
  var xs = [undefined, null];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2 + N / 4);
}
runTest(testUndefinedAndNull);

function testBoolean(n) {
  var xs = [true, false];
  var ys = [1, -1];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2 + N / 4);
}
runTest(testBoolean);
