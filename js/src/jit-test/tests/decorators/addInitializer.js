// |jit-test| skip-if: !getBuildConfiguration("decorators")
load(libdir + "asserts.js");

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
    context.addInitializer(() => {});
    // return undefined
  }
}

class C {
  @checkDecoratorContext("field", false, false, "x") x;
  @checkDecoratorContext("accessor", true, false, "y accessor storage") accessor y;
}

let c = new C();
