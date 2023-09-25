// |jit-test| skip-if: !getBuildConfiguration("decorators")

load(libdir + "asserts.js");

function assertAccessorDescriptor(object, name) {
  var desc = Object.getOwnPropertyDescriptor(object, name);
  assertEq(desc.configurable, true, "The value of `desc.configurable` is `true`");
  assertEq(desc.enumerable, false, "The value of `desc.enumerable` is `false`");
  assertEq(typeof desc.get, 'function', "`typeof desc.get` is `'function'`");
  assertEq(typeof desc.set, 'function', "`typeof desc.set` is `'function'`");
  assertEq(
    'prototype' in desc.get,
    false,
    "The result of `'prototype' in desc.get` is `false`"
  );
  assertEq(
    'prototype' in desc.set,
    false,
    "The result of `'prototype' in desc.set` is `false`"
  );
}

class C {
  accessor x;
}

assertAccessorDescriptor(C.prototype, 'x');

let c = new C();
assertEq(c.x, undefined, "The value of `c.x` is `undefined` after construction");
c.x = 2;
assertEq(c.x, 2, "The value of `c.x` is `2`, after executing `c.x = 2;`");

class D {
  accessor x = 1;
}

assertAccessorDescriptor(D.prototype, 'x');

let d = new D();
assertEq(d.x, 1, "The value of `d.x` is `1` after construction");
d.x = 2;
assertEq(d.x, 2, "The value of `d.x` is `2`, after executing `d.x = 2;`");

class E {
  accessor #x = 1;

  getX() {
    return this.#x;
  }

  setX(v) {
    this.#x = v;
  }
}

let e = new E();
assertEq(e.getX(), 1, "The value of `e.x` is `1` after construction");
e.setX(2);
assertEq(e.getX(), 2, "The value of `e.x` is `2`, after executing `e.setX(2);`");

class F {
  static accessor x = 1;
}

assertEq(F.x, 1, "The value of `F.x` is `1` after construction");
F.x = 2;
assertEq(F.x, 2, "The value of `F.x` is `2`, after executing `F.x = 2;`");

assertAccessorDescriptor(F, 'x');

class G {
  static accessor #x = 1;

  getX() {
    return G.#x;
  }

  setX(v) {
    G.#x = v;
  }
}
g = new G();
assertEq(g.getX(), 1, "The value of `g.getX()` is `1` after construction");
g.setX(2);
assertEq(g.getX(), 2, "The value of `g.getX()` is `2`, after executing `g.setX(2)`");
