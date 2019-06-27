class Base {
}

class C extends Base {
  field;
}
let c = new C();
assertEq(true, "field" in c);

var D = class extends Base {
  field;
};
let d = new D();
assertEq(true, "field" in d);

class E extends Base {
  field;
  constructor() {
    super();
  }
};
let e = new E();
assertEq(true, "field" in e);

class F extends Base {
  constructor() {
    super();
  }
  field;
};
let f = new F();
assertEq(true, "field" in f);

class G extends Base {
  field2 = 2;
  constructor() {
    super();
  }
  field3 = 3;
};
let g = new G();
assertEq(2, g.field2);
assertEq(3, g.field3);

class H extends Base {
  field = 2;
  constructor() {
    eval("super()");
  }
};
let h = new H();
assertEq(2, h.field);

class I extends Base {
  field = 2;
  constructor() {
      class Tmp {
          field = 10;
          [super()];
      }
  }
};
let i = new I();
assertEq(2, i.field);

class J extends Base {
  field = 2;
  constructor() {
      class Tmp {
          field = 10;
          [super()](){}
      }
  }
};
let j = new J();
assertEq(2, j.field);
