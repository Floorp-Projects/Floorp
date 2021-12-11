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

function testConstant(n) {
  var xs = ["a", "b"];
  var ys = ["c", "d"];
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
runTest(testConstant);

function testConstantFatInline(n) {
  var xs = ["a", "b"].map(s => s.repeat(10));
  var ys = ["c", "d"].map(s => s.repeat(10));
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
runTest(testConstantFatInline);

function testComputed(n) {
  var xs = ["a", "b"];
  var ys = ["c", "d"];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    z = String.fromCharCode(z.charCodeAt(0));
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2 + N / 4);
}
runTest(testComputed);

function testRope(n) {
  var xs = ["a", "b"];
  var ys = ["c", "d"];
  var zs = [...xs, ...ys];
  var map = createMap(xs.map(x => x.repeat(100)), n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3].repeat(100);
    var v = map.get(z);
    if (v !== undefined) c += v;
  }
  assertEq(c, N / 2 + N / 4);
}
runTest(testRope);
