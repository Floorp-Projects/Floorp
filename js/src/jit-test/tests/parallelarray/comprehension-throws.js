// |jit-test| error: TypeError;
function buildComprehension() {
  // Throws if elemental fun not callable
  var p = new ParallelArray([2,2], undefined);
}

buildComprehension();
