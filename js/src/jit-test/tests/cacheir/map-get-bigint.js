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

function testInlineDigitsSameSign(n) {
  var xs = [1n, 2n];
  var ys = [3n, 4n];
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
runTest(testInlineDigitsSameSign);

function testInlineDigitsDifferentSign(n) {
  var xs = [-1n, 2n];
  var ys = [1n, -2n];
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
runTest(testInlineDigitsDifferentSign);

function testHeapDigitsSameSign(n) {
  // Definitely uses heap digits.
  var heap = 2n ** 1000n;

  var xs = [heap + 1n, heap + 2n];
  var ys = [heap + 3n, heap + 4n];
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
runTest(testHeapDigitsSameSign);

function testHeapDigitsDifferentSign(n) {
  // Definitely uses heap digits.
  var heap = 2n ** 1000n;

  var xs = [-(heap + 1n), heap + 2n];
  var ys = [heap + 1n, -(heap + 2n)];
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
runTest(testHeapDigitsDifferentSign);
