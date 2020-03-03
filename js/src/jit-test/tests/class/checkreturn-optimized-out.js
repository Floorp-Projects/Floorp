function h() {}

class Base {}

class C extends Base {
  constructor(cond) {
    if (cond) {
      // Never entered.
      for (var i = 0; i < args.length; ++i) h();
    }
    super();
  }
}

function f() {
  for (var i = 0; i < 10; i++) {
    var x = new C(false);
  }
}

for (var i = 0; i < 3; i++) {
  f();
}
