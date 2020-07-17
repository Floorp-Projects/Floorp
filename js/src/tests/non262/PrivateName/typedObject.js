// |reftest| shell-option(--enable-private-fields)

class Base {
  constructor(o) {
    return o;
  }
}

class A extends Base {
  #x = 10;
  static hasx(o) {
    try {
      o.#x;
      return true;
    } catch {
      return false;
    }
  }
}

var struct = new TypedObject.StructType({foo: TypedObject.float32});
var target = new struct;

var stamped = false;
try {
  // #x Cannot be stamped into a typed object.
  new A(target);
  stamped = true;
} catch (e) {
  assertEq(e instanceof TypeError, true);
}
assertEq(stamped, false);
assertEq(A.hasx(target), false);

if (typeof reportCompare === 'function') reportCompare(0, 0);