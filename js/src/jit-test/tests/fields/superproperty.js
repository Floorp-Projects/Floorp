// SuperProperty syntax is allowed in fields.

class Base {
  get accessor() {
    return this.constructor.name;
  }
  method() {
    return this.constructor.name;
  }
}

class Derived extends Base {
  constructor() {
    super();
  }
  get accessor() {
    throw new Error("don't call this");
  }
  method() {
    throw new Error("don't call this");
  }
  field1 = super.accessor;
  field2 = super.method();
}

assertEq(new Derived().field1, "Derived");
assertEq(new Derived().field2, "Derived");

if (typeof reportCompare === "function") {
  reportCompare(true, true);
}
