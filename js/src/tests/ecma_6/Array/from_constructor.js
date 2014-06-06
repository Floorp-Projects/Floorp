/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Array.from can be applied to any constructor.
// For example, the Date builtin constructor.
var d = Array.from.call(Date, ["A", "B"]);
assertEq(Array.isArray(d), false);
assertEq(Object.prototype.toString.call(d), "[object Date]");
assertEq(Object.getPrototypeOf(d), Date.prototype);
assertEq(d.length, 2);
assertEq(d[0], "A");
assertEq(d[1], "B");

// Or Object.
var obj = Array.from.call(Object, []);
assertEq(Array.isArray(obj), false);
assertEq(Object.getPrototypeOf(obj), Object.prototype);
assertEq(Object.getOwnPropertyNames(obj).join(","), "length");
assertEq(obj.length, 0);

// Or any JS function.
function C(arg) {
    this.args = arguments;
}
var c = Array.from.call(C, {length: 1, 0: "zero"});
assertEq(c instanceof C, true);
assertEq(c.args.length, 1);
assertEq(c.args[0], 1);
assertEq(c.length, 1);
assertEq(c[0], "zero");

// If the 'this' value passed to Array.from is not a constructor,
// a plain Array is created.
var arr = [3, 4, 5];
var nonconstructors = [
    {}, Math, Object.getPrototypeOf, undefined, 17,
    () => ({})  // arrow functions are not constructors
];
for (var v of nonconstructors) {
    obj = Array.from.call(v, arr);
    assertEq(Array.isArray(obj), true);
    assertDeepEq(obj, arr);
}

// Array.from does not get confused if global.Array is replaced with another
// constructor.
function NotArray() {
}
var RealArray = Array;
NotArray.from = Array.from;
Array = NotArray;
assertEq(RealArray.from([1]) instanceof RealArray, true);
assertEq(NotArray.from([1]) instanceof NotArray, true);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
