class Test {
  constructor() {
    this.foo = {
      a: "foobar"
    };
  }

  bar() {
    console.log(this.foo.a);
  }
}
