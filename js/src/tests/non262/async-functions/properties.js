/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function assertOwnDescriptor(object, propertyKey, expected) {
  var desc = Object.getOwnPropertyDescriptor(object, propertyKey);
  if (desc === undefined) {
    assertEq(expected, undefined, "Property shouldn't be present");
    return;
  }

  assertEq(desc.enumerable, expected.enumerable, `${String(propertyKey)}.[[Enumerable]]`);
  assertEq(desc.configurable, expected.configurable, `${String(propertyKey)}.[[Configurable]]`);

  if (Object.prototype.hasOwnProperty.call(desc, "value")) {
    assertEq(desc.value, expected.value, `${String(propertyKey)}.[[Value]]`);
    assertEq(desc.writable, expected.writable, `${String(propertyKey)}.[[Writable]]`);
  } else {
    assertEq(desc.get, expected.get, `${String(propertyKey)}.[[Get]]`);
    assertEq(desc.set, expected.set, `${String(propertyKey)}.[[Set]]`);
  }
}

async function asyncFunc(){}
var AsyncFunctionPrototype = Object.getPrototypeOf(asyncFunc);
var AsyncFunction = AsyncFunctionPrototype.constructor;


// ES2017, 25.5.2 Properties of the AsyncFunction Constructor

assertEqArray(Object.getOwnPropertyNames(AsyncFunction).sort(), ["length", "name", "prototype"]);
assertEqArray(Object.getOwnPropertySymbols(AsyncFunction), []);

assertOwnDescriptor(AsyncFunction, "length", {
    value: 1, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(AsyncFunction, "name", {
    value: "AsyncFunction", writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(AsyncFunction, "prototype", {
    value: AsyncFunctionPrototype, writable: false, enumerable: false, configurable: false
});


// ES2017, 25.5.3 Properties of the AsyncFunction Prototype Object

assertEqArray(Object.getOwnPropertyNames(AsyncFunctionPrototype).sort(), ["constructor"]);
assertEqArray(Object.getOwnPropertySymbols(AsyncFunctionPrototype), [Symbol.toStringTag]);

assertOwnDescriptor(AsyncFunctionPrototype, "constructor", {
    value: AsyncFunction, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(AsyncFunctionPrototype, Symbol.toStringTag, {
    value: "AsyncFunction", writable: false, enumerable: false, configurable: true
});


// ES2017, 25.5.4 AsyncFunction Instances

assertEqArray(Object.getOwnPropertyNames(asyncFunc).sort(), ["length", "name"]);
assertEqArray(Object.getOwnPropertySymbols(asyncFunc), []);

assertOwnDescriptor(asyncFunc, "length", {
    value: 0, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(asyncFunc, "name", {
    value: "asyncFunc", writable: false, enumerable: false, configurable: true
});


if (typeof reportCompare == "function")
    reportCompare(true, true);
