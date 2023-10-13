var g = newGlobal({freezeBuiltins: true});

g.evaluate("" + function checkFrozen(name) {
  // Check constructor on the global is non-writable/non-configurable.
  let desc = Object.getOwnPropertyDescriptor(this, name);
  assertEq(desc.writable, false);
  assertEq(desc.configurable, false);

  // Constructor must be frozen.
  let ctor = desc.value;
  assertEq(Object.isFrozen(ctor), true);

  // Prototype must be sealed.
  if (ctor.prototype) {
    assertEq(Object.isSealed(ctor.prototype), true);
  }
});

g.checkFrozen("Object");
g.checkFrozen("Array");
g.checkFrozen("Function");

g.checkFrozen("ArrayBuffer");
g.checkFrozen("Int32Array");
g.checkFrozen("Number");
g.checkFrozen("String");
g.checkFrozen("Proxy");
g.checkFrozen("Promise");
g.checkFrozen("RegExp");
g.checkFrozen("Map");
g.checkFrozen("WeakMap");
g.checkFrozen("WeakRef");
g.checkFrozen("Error");
g.checkFrozen("TypeError");

g.checkFrozen("JSON");
g.checkFrozen("Math");
