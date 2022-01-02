class C extends class {} {
  constructor() {
    var f = () => super();

    try {
      return;
    } finally {
      f();
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
