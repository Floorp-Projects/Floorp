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

function testConstant(n) {
  var xs = ["a", "b"];
  var ys = ["c", "d"];
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
runTest(testConstant);

function testComputed(n) {
  var xs = ["a", "b"];
  var ys = ["c", "d"];
  var zs = [...xs, ...ys];
  var set = createSet(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    z = String.fromCharCode(z.charCodeAt(0));
    if (set.has(z)) c++;
  }
  assertEq(c, N / 2);
}
runTest(testComputed);

function testRope(n) {
  var xs = ["a", "b"];
  var ys = ["c", "d"];
  var zs = [...xs, ...ys];
  var set = createSet(xs.map(x => x.repeat(100)), n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3].repeat(100);
    if (set.has(z)) c++;
  }
  assertEq(c, N / 2);
}
runTest(testRope);
