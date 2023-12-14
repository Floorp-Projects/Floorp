// |jit-test| skip-if: !getBuildConfiguration("decorators")

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
    assertEq(typeof context.addInitializer, "undefined");
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

class E {
  static #dec1(value, context) {
    return class extends value {
      constructor(...args) {
        super(...args);
      }

      x2 = true;
    }
  }
  static {
    this.F = @E.#dec1 class {
      x1 = true;
    }
  }
}

let f = new E.F();
assertEq(f.x1, true);
assertEq(f.x2, true);

assertThrowsInstanceOf(() => {
  @(() => { return "hello!"; }) class G {}
}, TypeError), "Returning a value other than undefined or a callable throws.";

assertThrowsInstanceOf(() => {
  class G {
    static #dec1() {}
    static {
      @G.#dec1 class G {}
    }
  }
}, ReferenceError), "can't access lexical declaration 'G' before initialization";

const decoratorOrder = [];
function makeOrderedDecorator(order) {
  return function (value, context) {
    decoratorOrder.push(order);
    return value;
  }
}

@makeOrderedDecorator(1) @makeOrderedDecorator(2) @makeOrderedDecorator(3)
class H {}
assertEq(decoratorOrder.length, 3);
assertEq(decoratorOrder[0], 3);
assertEq(decoratorOrder[1], 2);
assertEq(decoratorOrder[2], 1);
