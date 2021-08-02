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

function test(n) {
  var xs = [{}, {}];
  var ys = [{}, {}];
  var zs = [...xs, ...ys];
  var map = createMap(xs, n);

  var N = 100;
  var c = 0;
  for (var i = 0; i < N; ++i) {
    var z = zs[i & 3];
    if (map.has(z)) c++;
  }
  assertEq(c, N / 2);
}
runTest(test);
