
function buildWithHoles() {
  // Test holes
  var a = new Array(5);
  for (var cnt = 0; cnt < a.length; cnt+=2) {
      a[cnt] = cnt;
  }
  var b = [0,1,2,3,4];
  var p = new ParallelArray(a);
  assertEq(Object.keys(p).join(","), Object.keys(b).join(","));
}

// FIXME(bug 844882) self-hosted object not array-like, exposes internal properties
// if (getBuildConfiguration().parallelJS)
//   buildWithHoles();
