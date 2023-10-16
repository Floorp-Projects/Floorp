// |jit-test| skip-if: !getBuildConfiguration()['decorators']

load(libdir + "asserts.js");

let dec1Called = false;

// This explicitly tests the case where undefined is returned.
function dec1(value, context) {
  dec1Called = true;
  // returns undefined
}

function dec2(value, context) {
  return class extends value {
    constructor(...args) {
      super(...args);
    }

    x2 = true;
  }
}

function checkDecoratorContext(name) {
  return function (value, context) {
    assertEq(typeof value, "function");
    assertEq(context.kind, "class");
    assertEq(context.name, name);
    assertEq(typeof context.addInitializer, "object");
    // return undefined
  }
}

@dec1 class C1 {};
assertEq(dec1Called, true);

@dec2 class C2 {
  x1 = true;
}

let c2 = new C2();
assertEq(c2.x1, true);
assertEq(c2.x2, true);

let c3 = new @dec2 class {
  x1 = true;
}

assertEq(c3.x1, true);
assertEq(c3.x2, true);

@checkDecoratorContext("D") class D {}
let d2 = new @checkDecoratorContext(undefined) class {};

assertThrowsInstanceOf(() => {
  @(() => { return "hello!"; }) class E {}
}, TypeError), "Returning a value other than undefined or a callable throws.";
