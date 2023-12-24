// |jit-test| skip-if: !getBuildConfiguration("decorators")

load(libdir + "asserts.js");

let dec1Called = false;

// This explicitly tests the case where undefined is returned.
function dec1(value, context) {
  dec1Called = true;
  // return undefined
}

function dec2(value, context) {
  return (x) => "replaced: " + x;
}

function dec3(value, context) {
  return (x) => "decorated: " + value.call(this, x);
}

let decorators = {
  "dec4": (value, context) => {
    return (x) => "decorated: " + x;
  },
  "more": {
    "deeply": {
      "nested": {
        "dec5": (value, context) => {
          return (x) => "decorated: " + x;
        }
      }
    }
  }
};

function dec6(arg) {
  return (value, context) => {
    return (x) => arg + " decorated: " + value.call(this, x);
  }
}

function checkDecoratorContext(kind, isPrivate, isStatic, name) {
  return (value, context) => {
    assertEq(typeof value.call, "function");
    assertEq(context.kind, kind);
    assertEq(typeof context.access, "object");
    assertEq(context.private, isPrivate);
    assertEq(context.static, isStatic);
    assertEq(context.name, name);
    if (isStatic) {
      assertEq(typeof context.addInitializer, "undefined");
    } else {
      assertEq(typeof context.addInitializer, "function");
    }
  }
}

class C {
  @dec1 f1(x) { return "called with: " + x; }
  @dec2 f2(x) { return "called with: " + x; }
  @dec1 @dec2 f3(x) { return "called with: " + x; }
  @dec1 @dec2 @dec3 f4(x) { return "called with: " + x; }
  @decorators.dec4 f5(x) { return "called with: " + x; }
  @decorators.more.deeply.nested.dec5 f6(x) { return "called with: " + x; }
  @(() => { }) f7(x) { return "called with: " + x; }
  @((value, context) => { return (x) => "decorated: " + x; }) f8(x) { return "called with: " + x; }
  @dec6("hello!") f9(x) { return "called with: " + x; }

  static dec7(value, context) { return function(x) { return "replaced: " + x; } }
  static #dec8(value, context) { return function(x) { return "replaced: " + x; } }

  static {
    this.D = class {
      @C.dec7 static f10(x) { return "called with: " + x; }
      @C.#dec8 static f11(x) { return "called with: " + x; }
    }
  }

  @checkDecoratorContext("method", false, false, "f12") f12(x) { return x; }
  @checkDecoratorContext("method", false, true, "f13") static f13(x) { return x; }
  @checkDecoratorContext("method", true, false, "#f14") #f14(x) { return x; }
}

let c = new C();
assertEq(dec1Called, true);
assertEq(c.f1("value"), "called with: value");
assertEq(c.f2("value"), "replaced: value");
assertEq(c.f3("value"), "replaced: value");
assertEq(c.f4("value"), "replaced: value");
assertEq(c.f5("value"), "decorated: value");
assertEq(c.f6("value"), "decorated: value");
assertEq(c.f7("value"), "called with: value");
assertEq(c.f8("value"), "decorated: value");
assertEq(c.f9("value"), "hello! decorated: called with: value");
assertEq(C.D.f10("value"), "replaced: value");
assertEq(C.D.f11("value"), "replaced: value");

assertThrowsInstanceOf(() => {
  class D {
    @(() => { return "hello!"; }) f(x) { return x; }
  }
}, TypeError), "Returning a value other than undefined or a callable throws.";

const decoratorOrder = [];
function makeOrderedDecorator(order) {
  return function (value, context) {
    decoratorOrder.push(order);
    return value;
  }
}

class E {
  @makeOrderedDecorator(1) @makeOrderedDecorator(2) @makeOrderedDecorator(3)
  f(x) { return x; }
}

let e = new E();
assertEq(e.f("value"), "value");
assertEq(decoratorOrder.length, 3);
assertEq(decoratorOrder[0], 3);
assertEq(decoratorOrder[1], 2);
assertEq(decoratorOrder[2], 1);
