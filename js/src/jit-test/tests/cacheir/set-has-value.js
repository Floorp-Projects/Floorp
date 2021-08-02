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

function testPolymorphic(n) {
  var xs = [10, 10.5, "test", Symbol("?"), 123n, -123n, {}, []];
  var ys = [-0, NaN, "bad", Symbol("!"), 42n, -99n, {}, []];
  var zs = [...xs, ...ys];
  var set = createSet(xs, n);

  var N = 128;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 15];
    if (set.has(z)) c++;
  }
  assertEq(c, N / 2);
}
runTest(testPolymorphic);
