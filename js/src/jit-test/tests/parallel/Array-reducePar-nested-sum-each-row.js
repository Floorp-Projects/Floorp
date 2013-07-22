load(libdir + "parallelarray-helpers.js");

function test() {
  var array1 = range(0, 512);
  var array2 = Array.build(512, function(i) {
    return i*1000000 + array1.reduce(sum);
  });

  assertParallelExecSucceeds(
    function (m) {
      return  Array.buildPar(512, function(i) {
        return i*1000000 + array1.reducePar(sum);
      });
    },
    function (r) {
      assertStructuralEq(array2, r);
    });

  function sum(a, b) { return a+b; }
}

if (getBuildConfiguration().parallelJS)
  test();
