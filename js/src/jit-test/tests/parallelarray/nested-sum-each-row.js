load(libdir + "parallelarray-helpers.js");

function test() {
  var array1 = range(0, 512);
  var array2 = build(512, function(i) {
    return i*1000000 + array1.reduce(sum);
  });


  var parray1 = new ParallelArray(array1);
  var parray2 = new ParallelArray(512, function(i) {
    return i*1000000 + parray1.reduce(sum);
  });

  assertStructuralEq(array2, parray2);

  function sum(a, b) { return a+b; }
}

test();
