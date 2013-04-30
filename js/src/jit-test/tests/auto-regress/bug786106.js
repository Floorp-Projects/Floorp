// Binary: cache/js-dbg-64-92b9b2840a79-linux
// Flags:
//

if (getBuildConfiguration().parallelJS) {
  var p = new ParallelArray([2, 3,, 4, 5, 6]);
  var r = p.scatter([0,1,0,3,4], 9, function (a,b) { return a+b; });
}
