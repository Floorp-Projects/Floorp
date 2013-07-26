load(libdir + "eqArrayHelper.js");

function testPartition() {
  var p = new ParallelArray([1,2,3,4,5,6,7,8]);
  var pp = p.partition(2);
  var ppp = pp.partition(2);
  var ppShape = [p.shape[0] / 2, 2].concat(p.shape.slice(1));
  var pppShape = [pp.shape[0] / 2, 2].concat(pp.shape.slice(1));
  assertEqArray(pp.shape, ppShape);
  assertEq(pp.toString(), "<1,2>,<3,4>,<5,6>,<7,8>");
  assertEqArray(ppp.shape, pppShape);
  assertEq(ppp.toString(), "<<1,2>,<3,4>>,<<5,6>,<7,8>>");
}

if (getBuildConfiguration().parallelJS) testPartition();
