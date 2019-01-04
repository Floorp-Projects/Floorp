class Test {
  constructor() {
    this.foo = "foo";
  }

  bar(a) {
    console.log("bar", a);
  }

  baz = b => {
    return b * 2;
  };
}

class Test2 {}

let expressiveClass = class {};
