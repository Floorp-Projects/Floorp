load(libdir + "parallelarray-helpers.js");

function makeObject(e, i, c) {
  var v = {element: e, index: i, collection: c};

  if (e == 512) // note: happens once
    delete v.index;

  return v;
}

function test() {
  var array = range(0, 768);
  var array1 = array.map(makeObject);

  assertParallelExecWillRecover(function (m) {
    var pa = new ParallelArray(array);
    var pa1 = pa.map(makeObject, m);
    assertStructuralEq(pa1, array1);
  });
}

if (getBuildConfiguration().parallelJS)
  test();
