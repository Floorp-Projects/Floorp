function testEnumerate() {
  var p = new ParallelArray([1,2,3,4,5]);
  for (var i in p)
    assertEq(i >= 0 && i < p.length, true);
}

// FIXME(bug 844882) self-hosted object not array-like, exposes internal properties
// testEnumerate();
