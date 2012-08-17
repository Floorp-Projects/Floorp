// |jit-test| error: TypeError;
function testMap() {
  // Test map throws
  var p = new ParallelArray([1,2,3,4])
  var m = p.map(42);
}

testMap();

