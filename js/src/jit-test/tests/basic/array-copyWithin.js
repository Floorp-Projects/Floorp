// Tests for Array#copyWithin

load(libdir + "asserts.js");

assertEq(Array.prototype.copyWithin.length, 2);

// works with two arguments
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, 3), [4, 5, 3, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(1, 3), [1, 4, 5, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(1, 2), [1, 3, 4, 5, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(2, 2), [1, 2, 3, 4, 5]);

// works with three arguments
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, 3, 4), [4, 2, 3, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(1, 3, 4), [1, 4, 3, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(1, 2, 4), [1, 3, 4, 4, 5]);

// works with negative arguments
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, -2), [4, 5, 3, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, -2, -1), [4, 2, 3, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(-4, -3, -2), [1, 3, 3, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(-4, -3, -1), [1, 3, 4, 4, 5]);
assertDeepEq([1, 2, 3, 4, 5].copyWithin(-4, -3), [1, 3, 4, 5, 5]);

// works with array-like objects
var args = (function () { return Array.prototype.slice.call(arguments); }(1, 2, 3));
var argsClass = Object.prototype.toString.call(args);
assertDeepEq(args, [1, 2, 3]);
Array.prototype.copyWithin.call(args, -2, 0);
assertDeepEq(args, [1, 1, 2]);
assertDeepEq(Object.prototype.toString.call(args), argsClass);

// throws on null/undefined values
assertThrowsInstanceOf(function() {
  Array.prototype.copyWithin.call(null, 0, 3);
}, TypeError, "Assert that copyWithin fails if this value is null");

assertThrowsInstanceOf(function() {
  Array.prototype.copyWithin.call(undefined, 0, 3);
}, TypeError, "Assert that copyWithin fails if this value is undefined");

// test with this value as string
assertThrowsInstanceOf(function() {
  Array.prototype.copyWithin.call("hello world", 0, 3);
}, TypeError, "Assert that copyWithin fails if this value is string");

// test with this value as number
assertDeepEq(Array.prototype.copyWithin.call(34, 0, 3), new Number(34));

// test with this value as TypedArray
var buffer = new ArrayBuffer(16);
var int32View = new Int32Array(buffer);
for (var i=0; i<int32View.length; i++) {
  int32View[i] = i*2;
}
assertDeepEq(Array.prototype.copyWithin.call(int32View, 0, 1), new Int32Array([2, 4, 6, 6]));

// if arguments object is sloppy, copyWithin must move the arguments around
function f(a, b, c, d, e) {
  [].copyWithin.call(arguments, 1, 3);
  return [a, b, c, d, e];
}
assertDeepEq(f(1, 2, 3, 4, 5), [1, 4, 5, 4, 5]);

// test with target > start on 2 arguments
assertDeepEq([1, 2, 3, 4, 5].copyWithin(3, 0), [1, 2, 3, 1, 2]);

// test with target > start on 3 arguments
assertDeepEq([1, 2, 3, 4, 5].copyWithin(3, 0, 4), [1, 2, 3, 1, 2]);

// test on array with holes
var arr = new Array(6);
for (var i = 0; i < arr.length; i += 2) {
  arr[i] = i;
}
assertDeepEq(arr.copyWithin(0, 3), [, 4, , , 4, , ]);

// test on fractional arguments
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0.2, 3.9), [4, 5, 3, 4, 5]);

// test with -0
assertDeepEq([1, 2, 3, 4, 5].copyWithin(-0, 3), [4, 5, 3, 4, 5]);

// test with arguments more than this.length
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, 7), [1, 2, 3, 4, 5]);

// test with arguments less than -this.length
assertDeepEq([1, 2, 3, 4, 5].copyWithin(-7, 0), [1, 2, 3, 4, 5]);

// test with arguments equal to -this.length
assertDeepEq([1, 2, 3, 4, 5].copyWithin(-5, 0), [1, 2, 3, 4, 5]);

// test on empty array
assertDeepEq([].copyWithin(0, 3), []);

// test with target range being shorter than end - start
assertDeepEq([1, 2, 3, 4, 5].copyWithin(2, 1, 4), [1, 2, 2, 3, 4]);

// test overlapping ranges
arr = [1, 2, 3, 4, 5];
arr.copyWithin(2, 1, 4);
assertDeepEq(arr.copyWithin(2, 1, 4), [1, 2, 2, 2, 3]);

// check that delete is strict
arr = [1, , 3, , 4, 5];
Object.freeze(arr);
assertThrowsInstanceOf(function() {
  arr.copyWithin(2, 1, 4);
}, TypeError, "Assert that delete is strict in copyWithin");

// test with a proxy object
var proxyObj = {
  get: function(recipient, name) {
    return recipient[name] + 2;
  }
};

var p = new Proxy([1, 2, 3, 4, 5], proxyObj);
assertDeepEq(Array.prototype.copyWithin.call(p, 0, 3), [6, 7, , , 5]);

// test if we throw in between
arr = [1, 2, 3, 4, 5];
Object.defineProperty(arr, 1, {
  set: function () {
    throw new Error("Boom!");
  }
});

assertThrowsInstanceOf(function() {
  arr.copyWithin(1, 3);
}, Error, "Throwing in between.");
assertEq(arr[0], 1);
assertEq(arr[1], undefined);
assertEq(arr[2], 3);
assertEq(arr[3], 4);
assertEq(arr[4], 5);

// undefined as third argument
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, 3, undefined), [4, 5, 3, 4, 5]);

// test that this.length is called only once
arr = {0: 1, 1: 2, 2: 3, 3: 4, 4: 5};
var count = 0;
Object.defineProperty(arr, "length", {
  get: function () {
    count++;
  }
});
Array.prototype.copyWithin.call(arr, 1, 3);
assertEq(count, 1);

count = 0;
Array.prototype.copyWithin.call(arr, 1, 3, 4);
assertEq(count, 1);

var large = 10000;

// test on a large array
arr = new Array(large);
assertDeepEq(arr.copyWithin(45, 900), arr);

// test on floating point numbers
for (var i = 0; i < large; i++) {
  arr[i] = Math.random();
}
arr.copyWithin(45, 900);

// test on array of objects
for (var i = 0; i < large; i++) {
  arr[i] = { num: Math.random() };
}
arr.copyWithin(45, 900);

// test array length remains same
assertEq(arr.length, large);

// test null on third argument is handled correctly
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, 3, null), [1, 2, 3, 4, 5]);

// tamper the global Object prototype and test this works
Object.prototype[2] = 1;
assertDeepEq([1, 2, 3, 4, 5].copyWithin(0, 3), [4, 5, 3, 4, 5]);
