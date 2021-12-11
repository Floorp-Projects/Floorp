load(libdir + "asserts.js");

var error = {};

var iter = {
  [Symbol.iterator]() { return this; },
  next() { return {done: false}; },
  return() { throw error; },
};

class C extends class {} {
  constructor() {
    super();

    for (var k of iter) {
      return 0;
    }
  }
}

function test() {
  for (var i = 0; i < 100; ++i) {
    assertThrowsValue(() => new C(), error);
  }
}

test();
