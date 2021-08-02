// Ensure string hashing works correctly under gczeal=compact.

gczeal(14)

function test() {
  var set = new Set();
  var c = 0;
  var N = 1000;
  for (var i = 0; i < N; ++i) {
    var k = String.fromCodePoint(i);
    set.add(k);
    if (set.has(k)) c++;
  }
  assertEq(c, N);
}

test();
