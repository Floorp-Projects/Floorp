// |reftest| 
// AllPrivateIdentifiersValid uses only lexical string names, not
// the dynamic private names; which means the below is not a syntax
// error but is instead a TypeError on access.

class A {
  #x = 10;
  f() {
    class B {
      g() {
        return this.#x;  // note: #x isn't declared in this class, but
                         // the enclosing.
      }
    };
    this.y = new B;
  }
  constructor() {
    this.f();
  }
  g() {
    return this.y.g();
  }
};

a = new A;
try {
  a.g();
} catch (e) {
  assertEq(e instanceof TypeError, true);
}

if (typeof reportCompare === 'function') reportCompare(0, 0);