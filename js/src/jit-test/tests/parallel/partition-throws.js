load(libdir + "asserts.js");

function testPartitionDivisible() {
  var p = new ParallelArray([1,2,3,4]);
  var pp;
  assertThrowsInstanceOf(function () { pp = p.partition(3); }, Error);
  assertThrowsInstanceOf(function () { pp = p.partition(.34); }, Error);
}

if (getBuildConfiguration().parallelJS) testPartitionDivisible();
