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

function testInlineDigitsSameSign(n) {
  var xs = [1n, 2n];
  var ys = [3n, 4n];
  var zs = [...xs, ...ys];
  var set = createSet(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set.has(z)) c++;
  }
  assertEq(c, N / 2);
}
runTest(testInlineDigitsSameSign);

function testInlineDigitsDifferentSign(n) {
  var xs = [-1n, 2n];
  var ys = [1n, -2n];
  var zs = [...xs, ...ys];
  var set = createSet(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (set.has(z)) c++;
  }
  assertEq(c, N / 2);
}
runTest(testInlineDigitsDifferentSign);

function testHeapDigitsSameSign(n) {
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
  }
  assertEq(c, N / 2);
}
runTest(testHeapDigitsSameSign);

function testHeapDigitsDifferentSign(n) {
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
  }
  assertEq(c, N / 2);
}
runTest(testHeapDigitsDifferentSign);
