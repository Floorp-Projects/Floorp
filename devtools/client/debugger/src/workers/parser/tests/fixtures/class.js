class Test {
  publicProperty;
  #privateProperty = "default";
  static myStatic = "static";
  static hello() {
    return "Hello " + this.myStatic
  }
  static {
    const x = this.myStatic;
  }

  constructor() {
    this.publicProperty = "foo";
    this.#privateProperty = "bar";
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
