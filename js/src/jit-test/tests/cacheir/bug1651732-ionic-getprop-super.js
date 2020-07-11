class Base {
  static a = 0;
  static [Symbol.iterator] = 0;
}

class Derived extends Base {
  static m(key) {
    // Attach an IC through IonGetPropSuperIC.
    return super[key];
  }
}

var key = {
  value: "a",

  [Symbol.toPrimitive]() {
    return this.value;
  }
};

for (var i = 0; i < 100; ++i) {
  // Change key[Symbol.toPrimitive] to return a symbol after some warm-up.
  if (i > 80) {
    key.value = Symbol.iterator;
  }

  assertEq(Derived.m(key), 0);
}
