// Test the Error.prototype.stack getter/setter with various "fun" edge cases.

load(libdir + "asserts.js");

// Stack should be accessible in subclasses. The accessor should walk up the
// prototype chain.
assertEq(typeof Object.create(Error()).stack, "string");
assertEq(Object.create(Error.prototype).stack, "");

// Stack should be overridable in subclasses.
{
  let myError = Object.create(Error());
  myError.stack = 5;
  assertEq(myError.stack, 5);

  let myOtherError = Object.create(Error.prototype);
  myOtherError.stack = 2;
  assertEq(myOtherError.stack, 2);
}

// Should throw when there is no Error in the `this` object's prototype chain.
var obj = Object.create(null);
var desc = Object.getOwnPropertyDescriptor(Error.prototype, "stack");
Object.defineProperty(obj, "stack", desc);
assertThrowsInstanceOf(() => obj.stack, TypeError);

// Should throw with non-object `this` values.
assertThrowsInstanceOf(desc.set,                TypeError);
assertThrowsInstanceOf(desc.set.bind("string"), TypeError);
assertThrowsInstanceOf(desc.get,                TypeError);
assertThrowsInstanceOf(desc.get.bind("string"), TypeError);
