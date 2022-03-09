assertEq('WeakRef' in this, true);

function checkPropertyDescriptor(obj, property, writable, enumerable,
                                 configurable) {
  let desc = Object.getOwnPropertyDescriptor(obj, property);
  assertEq(typeof desc, "object");
  assertEq(desc.writable, writable);
  assertEq(desc.enumerable, enumerable);
  assertEq(desc.configurable, configurable);
}

function assertThrowsTypeError(thunk) {
  let error;
  try {
      thunk();
  } catch (e) {
      error = e;
  }
  assertEq(error instanceof TypeError, true);
}

assertEq(typeof this.WeakRef, "function");

// https://tc39.es/proposal-weakrefs/

// 1.1.1.1
// If NewTarget is undefined, throw a TypeError exception.
assertThrowsTypeError(() => new WeakRef());

// 1.1.1.2
// If Type(target) is not Object, throw a TypeError exception.
assertThrowsTypeError(() => new WeakRef(1));
assertThrowsTypeError(() => new WeakRef(true));
assertThrowsTypeError(() => new WeakRef("string"));
assertThrowsTypeError(() => new WeakRef(Symbol()));
assertThrowsTypeError(() => new WeakRef(null));
assertThrowsTypeError(() => new WeakRef(undefined));
new WeakRef({});

// 1.2
// The WeakRef constructor has a [[Prototype]] internal slot whose value is the
// intrinsic object %FunctionPrototype%.
assertEq(Object.getPrototypeOf(WeakRef), Function.prototype);

// 1.2.1
// The initial value of WeakRef.prototype is the intrinsic %WeakRefPrototype%
// object.
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false
// , [[Configurable]]: false }.
checkPropertyDescriptor(WeakRef, 'prototype', false, false, false);

// 1.3
// The WeakRef prototype object has a [[Prototype]] internal slot whose value is
// the intrinsic object %ObjectPrototype%.
let proto = WeakRef.prototype;
assertEq(Object.getPrototypeOf(proto), Object.prototype);

// 1.3.1
// The initial value of WeakRef.prototype.constructor is the intrinsic object
// %WeakRef%.
assertEq(proto.constructor, WeakRef);

// 1.3.2
// WeakRef.prototype.deref ()
assertEq(proto.hasOwnProperty('deref'), true);
assertEq(typeof proto.deref, 'function');

// 1.3.3
// The initial value of the @@toStringTag property is the String value
// "WeakRef".
// This property has the attributes { [[Writable]]: false, [[Enumerable]]: false,
// [[Configurable]]: true }.
assertEq(proto[Symbol.toStringTag], "WeakRef");
checkPropertyDescriptor(proto, Symbol.toStringTag, false, false, true);

// 1.4
// WeakRef instances are ordinary objects that inherit properties from the
// WeakRef prototype
let weakRef = new WeakRef({});
assertEq(Object.getPrototypeOf(weakRef), proto);

