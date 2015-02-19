"use strict";

// Seal
assertEq(Object.isSealed(new Int32Array(2)), false);
assertEq(Object.isSealed(new Int32Array(0)), false);

var array = new Int32Array(0);
Object.preventExtensions(array);
assertEq(Object.isSealed(array), true);

array = new Int32Array(1);
array.b = "test";
Object.preventExtensions(array);
assertEq(Object.isSealed(array), false);
Object.defineProperty(array, "b", {configurable: false});
assertEq(Object.isSealed(array), true);

array = new Int32Array(2);
array.b = "test";
Object.seal(array);
assertEq(Object.isSealed(array), true);
assertThrowsInstanceOf(() => array.c = 15, TypeError);

// Freeze
assertEq(Object.isFrozen(new Int32Array(2)), false);
assertEq(Object.isFrozen(new Int32Array(0)), false);

// Empty non-extensible typed-array is trvially frozen
var array = new Int32Array(0);
Object.preventExtensions(array);
assertEq(Object.isFrozen(array), true);

array = new Int32Array(0);
array.b = "test";
assertEq(Object.isFrozen(array), false);
Object.preventExtensions(array);
assertEq(Object.isFrozen(array), false);
Object.defineProperty(array, "b", {configurable: false, writable: false});
assertEq(Object.isFrozen(array), true);

// Non-empty typed arrays can never be frozen, because the elements stay writable
array = new Int32Array(1);
assertThrowsInstanceOf(() => Object.freeze(array), TypeError);
assertEq(Object.isExtensible(array), false);
assertEq(Object.isFrozen(array), false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
