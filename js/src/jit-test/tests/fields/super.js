// |jit-test| --enable-experimental-fields

class Base {
}

class C extends Base {
  field;
}
new C();

var D = class extends Base {
  field;
};
new D();

class E extends Base {
  field;
  constructor() {
    super();
  }
};
new E();

class F extends Base {
  constructor() {
    super();
  }
  field;
};
new F();

class G extends Base {
  field2 = 2;
  constructor() {
    super();
  }
  field3 = 3;
};
new G();

class H extends Base {
  field = 2;
  constructor() {
    eval("super()");
  }
};
new H();

class I extends Base {
  field = 2;
  constructor() {
      class Tmp {
          [super()];
      }
  }
};
new I();
