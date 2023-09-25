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

function dec4(arg) {
  return (value, context) => {
    return (x) => arg + " decorated: " + value.call(this, x);
  }
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

function checkDecoratorContext(kind, isPrivate, isStatic, name) {
  return (value, context) => {
    assertEq(typeof value.call, "function");
    assertEq(context.kind, kind);
    assertEq(typeof context.access, "object");
    assertEq(context.private, isPrivate);
    assertEq(context.static, isStatic);
    assertEq(context.name, name);
    assertEq(typeof context.addInitializer, "object");
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
  @dec4("hello!") f9(x) { return "called with: " + x; }

  @checkDecoratorContext("method", false, false, "f10") f10(x) { return x; }
  @checkDecoratorContext("method", false, true, "f11") static f11(x) { return x; }
  @checkDecoratorContext("method", true, false, "#f12") #f12(x) { return x; }
}

let c = new C();
assertEq(dec1Called, true);
assertEq(c.f1("value"), "called with: value");
assertEq(c.f2("value"), "replaced: value");
assertEq(c.f3("value"), "replaced: value");
assertEq(c.f4("value"), "decorated: replaced: value");
assertEq(c.f5("value"), "decorated: value");
assertEq(c.f6("value"), "decorated: value");
assertEq(c.f7("value"), "called with: value");
assertEq(c.f8("value"), "decorated: value");
assertEq(c.f9("value"), "hello! decorated: called with: value");

assertThrowsInstanceOf(() => {
  class D {
    @(() => { return "hello!"; }) f(x) { return x; }
  }
}, TypeError), "Returning a value other than undefined or a callable throws.";
