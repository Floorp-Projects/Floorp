// |jit-test| skip-if: !getBuildConfiguration("decorators")
load(libdir + "asserts.js");

let extraInitializerCalled = {};

function checkDecoratorContext(kind, isPrivate, isStatic, name) {
  return function (value, context) {
    if (kind == "field") {
      assertEq(value, undefined);
    } else if (kind == "accessor") {
      assertEq(typeof value, "object");
      assertEq(typeof value.get, "function");
      assertEq(typeof value.set, "function");
    }
    assertEq(context.kind, kind);
    assertEq(typeof context.access, "object");
    assertEq(context.private, isPrivate);
    assertEq(context.static, isStatic);
    assertEq(context.name, name);
    assertEq(typeof context.addInitializer, "function");
    context.addInitializer(() => {extraInitializerCalled[context.name] = true;});
    // return undefined
  }
}

class C {
  @checkDecoratorContext("field", false, false, "x") x;
  @checkDecoratorContext("accessor", true, false, "y accessor storage") accessor y;
  @checkDecoratorContext("method", false, false, "f") f() {};
  @checkDecoratorContext("method", false, false, 1) 1() {};
  @checkDecoratorContext("method", false, false, 2) 2n() {};
}

let c = new C();
assertEq(extraInitializerCalled["x"], true);
assertEq(extraInitializerCalled["y accessor storage"], true);
assertEq(extraInitializerCalled["f"], true);
assertEq(extraInitializerCalled["1"], true);
assertEq(extraInitializerCalled["2"], true);
