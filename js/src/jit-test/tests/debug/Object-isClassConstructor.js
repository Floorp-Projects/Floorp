// Debugger.Object.prototype.isClassConstructor recognizes ES6 classes.

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger();
var gDO = dbg.addDebuggee(g);
var hits = 0;

function checkIsClassConstructor(shouldBe, expr) {
  print(expr);
  assertEq(gDO.executeInGlobal(expr).return.isClassConstructor, shouldBe);
}

checkIsClassConstructor(true, "class MyClass{}; MyClass;");
checkIsClassConstructor(false, "class MyClass2{}; MyClass2.constructor;");
checkIsClassConstructor(
  false,
  "class MyClass3{}; Object.getPrototypeOf(MyClass3)"
);
checkIsClassConstructor(false, "(a) => { bleh; }");
checkIsClassConstructor(false, "(async function f () {})");
checkIsClassConstructor(void 0, "({})");
