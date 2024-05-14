function makeProxy(n) {
  return new Proxy({}, {
    get() { return n; }
  })
}

var arr = [];
for (var i = 0; i < 100; i++) {
  arr.push(makeProxy(i));
}

// Test that proxy get traps with the same script but different
// functions call the correct target.
for (var i = 0; i < 500; i++) {
  var idx = i % arr.length;
  assertEq(arr[idx].x, idx);
}
