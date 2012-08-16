// |jit-test| error: Error;

function testPartitionDivisible() {
  var p = new ParallelArray([1,2,3,4]);
  var pp = p.partition(3);
}

testPartitionDivisible();
