class B {
  constructor(obj) { return obj; }
}

class C extends B {
  #f = 1;
  static m(obj) {
    obj.#f = new C(obj);  // ok, obj.#f brand check happens after RHS is evaluated
    assertEq(obj.#f, obj);
  }
}

C.m({});
