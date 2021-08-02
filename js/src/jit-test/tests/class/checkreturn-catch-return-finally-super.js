class C extends class {} {
  constructor() {
    try {
      throw null;
    } catch {
      return;
    } finally {
      super();
    }
  }
}

function test() {
  for (var i = 0; i < 100; ++i) {
    // No error.
    new C();
  }
}

test();
