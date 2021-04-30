class Base {
  constructor(o) {
    return o;
  }
}

class A extends Base {
  #x = 10;
  static gx(o) {
    return o.#x
  }
  static sx(o, v) {
    o.#x = v;
  }
}

function transplantTest(transplantOptions, global) {
  var {object, transplant} = transplantableObject(transplantOptions);

  new A(object);
  assertEq(A.gx(object), 10);
  A.sx(object, 15);
  assertEq(A.gx(object), 15);

  transplant(global);

  assertEq(A.gx(object), 15);
  A.sx(object, 29);
  assertEq(A.gx(object), 29);
}

// Structure helpfully provided by bug1403679.js
const thisGlobal = this;
const otherGlobalSameCompartment = newGlobal({sameCompartmentAs: thisGlobal});
const otherGlobalNewCompartment = newGlobal({newCompartment: true});

const globals =
    [thisGlobal, otherGlobalSameCompartment, otherGlobalNewCompartment];

function testWithOptions(fn) {
  for (let global of globals) {
    for (let options of [{}, {proxy: true}, {object: new FakeDOMObject()}, ]) {
      fn(options, global);
    }
  }
}

testWithOptions(transplantTest)