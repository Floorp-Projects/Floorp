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

function* generator(){}
var GeneratorFunctionPrototype = Object.getPrototypeOf(generator);
var GeneratorFunction = GeneratorFunctionPrototype.constructor;
var GeneratorPrototype = GeneratorFunctionPrototype.prototype;


// ES2017, 25.2.2 Properties of the GeneratorFunction Constructor

assertEqArray(Object.getOwnPropertyNames(GeneratorFunction).sort(), ["length", "name", "prototype"]);
assertEqArray(Object.getOwnPropertySymbols(GeneratorFunction), []);

assertOwnDescriptor(GeneratorFunction, "length", {
    value: 1, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorFunction, "name", {
    value: "GeneratorFunction", writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorFunction, "prototype", {
    value: GeneratorFunctionPrototype, writable: false, enumerable: false, configurable: false
});


// ES2017, 25.2.3 Properties of the GeneratorFunction Prototype Object

assertEqArray(Object.getOwnPropertyNames(GeneratorFunctionPrototype).sort(), ["constructor", "prototype"]);
assertEqArray(Object.getOwnPropertySymbols(GeneratorFunctionPrototype), [Symbol.toStringTag]);

assertOwnDescriptor(GeneratorFunctionPrototype, "constructor", {
    value: GeneratorFunction, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorFunctionPrototype, "prototype", {
    value: GeneratorPrototype, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorFunctionPrototype, Symbol.toStringTag, {
    value: "GeneratorFunction", writable: false, enumerable: false, configurable: true
});


// ES2017, 25.2.4 GeneratorFunction Instances

assertEqArray(Object.getOwnPropertyNames(generator).sort(), ["length", "name", "prototype"]);
assertEqArray(Object.getOwnPropertySymbols(generator), []);

assertOwnDescriptor(generator, "length", {
    value: 0, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(generator, "name", {
    value: "generator", writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(generator, "prototype", {
    value: generator.prototype, writable: true, enumerable: false, configurable: false
});


// ES2017, 25.3.1 Properties of Generator Prototype

assertEqArray(Object.getOwnPropertyNames(GeneratorPrototype).sort(), ["constructor", "next", "return", "throw"]);
assertEqArray(Object.getOwnPropertySymbols(GeneratorPrototype), [Symbol.toStringTag]);

assertOwnDescriptor(GeneratorPrototype, "constructor", {
    value: GeneratorFunctionPrototype, writable: false, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorPrototype, "next", {
    value: GeneratorPrototype.next, writable: true, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorPrototype, "return", {
    value: GeneratorPrototype.return, writable: true, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorPrototype, "throw", {
    value: GeneratorPrototype.throw, writable: true, enumerable: false, configurable: true
});

assertOwnDescriptor(GeneratorPrototype, Symbol.toStringTag, {
    value: "Generator", writable: false, enumerable: false, configurable: true
});


if (typeof reportCompare == "function")
    reportCompare(true, true);
