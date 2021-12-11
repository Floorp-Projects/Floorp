// Ensure private fields are stamped in order and that
// we can successfully partially initialize objects.

class Base {
  constructor(o) {
    return o;
  }
}

let constructorThrow = false;

function maybeThrow() {
  if (constructorThrow) {
    throw 'fail'
  }
  return 'sometimes'
}

class A extends Base {
  constructor(o) {
    super(o);
    constructorThrow = !constructorThrow;
  }

  #x = 'always';
  #y = maybeThrow();

  static gx(o) {
    return o.#x;
  }

  static gy(o) {
    return o.#y;
  }
};

var obj1 = {};
var obj2 = {};

new A(obj1);

var threw = true;
try {
  new A(obj2);
  threw = false;
} catch (e) {
  assertEq(e, 'fail');
}
assertEq(threw, true);

A.gx(obj1)
A.gx(obj2);  // Both objects get x;
A.gy(obj1);  // obj1 gets y

try {
  A.gy(obj2);  // shouldn't have x.
  threw = false;
} catch (e) {
  assertEq(e instanceof TypeError, true);
}

assertEq(threw, true);
