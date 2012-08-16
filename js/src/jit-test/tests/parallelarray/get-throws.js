// |jit-test| error: TypeError;

function testGetThrows() {
  // Throw if argument not object
  var p = new ParallelArray([1,2,3,4]);
  p.get(42);
}

testGetThrows();
