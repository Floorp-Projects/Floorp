var shouldBeThis;

class A {
  #nullReturn = false;
  constructor(nullReturn) {
    this.#nullReturn = nullReturn;
  }

  #calls = 0;

  #z =
      () => {
        assertEq(this, shouldBeThis);
        this.#calls++;

        // To test the second optional below.
        if (this.#nullReturn && this.#calls == 2) {
          return null;
        }

        return this;
      }

  static chainTest(o) {
    o?.#z().#z()?.#z();
  }
};

for (var i = 0; i < 1000; i++) {
  var a = new A();
  shouldBeThis = a;

  A.chainTest(a);
  A.chainTest(null);

  var b = new A(true);
  shouldBeThis = b;
  A.chainTest(b);
}
