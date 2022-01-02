load(libdir + "asserts.js");

class C extends class {} {
  constructor() {
    super();

    try {
      return 0;
    } catch {
      return;
    }
  }
}

function test() {
  for (var i = 0; i < 100; ++i) {
    assertThrowsInstanceOf(() => new C(), TypeError);
  }
}

test();
