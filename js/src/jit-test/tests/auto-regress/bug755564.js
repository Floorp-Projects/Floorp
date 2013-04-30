// Binary: cache/js-dbg-64-50177d59c0e1-linux
// Flags:
//

if (getBuildConfiguration().parallelJS) {
  var p = new ParallelArray([1,2,3,4,5]);
  var r = p.scatter([0,1,0,3,4], 9, function (a,b) { return a+b; });
  assertEq(r.toString( 5 ? r : 0, gc()) ,[4,2,9,4,5].join(","));
}
