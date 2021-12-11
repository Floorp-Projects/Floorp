function assertThrows(fun, errorType) {
  try {
    fun();
    throw 'Expected error, but none was thrown';
  } catch (e) {
    if (!(e instanceof errorType)) {
      throw 'Wrong error type thrown';
    }
  }
}

class A {
  #a;
  #b;
  #c;
  #d;
  #e;
  static destructure(o, x) {
    [o.#a, o.#b, o.#c, o.#d, ...o.#e] = x;
  }

  static get(o) {
    return {a: o.#a, b: o.#b, c: o.#c, d: o.#d, e: o.#e};
  }
};

for (var i = 0; i < 1000; i++) {
  var a = new A();
  A.destructure(a, [1, 2, 3, 4, 5]);
  var res = A.get(a);
  assertEq(res.a, 1);
  assertEq(res.b, 2);
  assertEq(res.c, 3);
  assertEq(res.d, 4);
  assertEq(res.e.length, 1);
  assertEq(res.e[0], 5);

  var obj = {};
  assertThrows(() => A.destructure(obj, [1, 2, 3, 4, 5]), TypeError);
  assertThrows(() => A.get(obj), TypeError);
}