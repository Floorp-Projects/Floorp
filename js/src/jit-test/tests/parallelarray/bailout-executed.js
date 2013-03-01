load(libdir + "parallelarray-helpers.js");

function makeObject(e, i, c) {
  var v = {element: e, index: i, collection: c};

  if (e == 512) // note: happens once
    delete v.i;

  return v;
}

function test() {
  var array = range(0, 768);
  var array1 = array.map(makeObject);

  var pa = new ParallelArray(array);
  var pa1 = pa.map(makeObject, {mode: "par", expect: "mixed"});

  assertStructuralEq(pa1, array1);
}

test();
