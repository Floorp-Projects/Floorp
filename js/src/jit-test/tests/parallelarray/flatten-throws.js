// |jit-test| error: Error;

function testFlattenFlat() {
  // Throw on flattening flat array
  var p = new ParallelArray([1]);
  var f = p.flatten();
}

testFlattenFlat();
