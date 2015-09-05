var BUGNUMBER = 887016;
var summary = "Implement RegExp.prototype[@@match].";

print(BUGNUMBER + ": " + summary);

assertEq(RegExp.prototype[Symbol.match].name, "[Symbol.match]");
assertEq(RegExp.prototype[Symbol.match].length, 1);
var desc = Object.getOwnPropertyDescriptor(RegExp.prototype, Symbol.match);
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, true);

var re = /a/;
var v = re[Symbol.match]("abcAbcABC");
assertEq(Array.isArray(v), true);
assertEq(v.length, 1);
assertEq(v[0], "a");

re = /d/;
v = re[Symbol.match]("abcAbcABC");
assertEq(v, null);

re = /a/ig;
v = re[Symbol.match]("abcAbcABC");
assertEq(Array.isArray(v), true);
assertEq(v.length, 3);
assertEq(v[0], "a");
assertEq(v[1], "A");
assertEq(v[2], "A");

re = /d/g;
v = re[Symbol.match]("abcAbcABC");
assertEq(v, null);

if (typeof reportCompare === "function")
    reportCompare(true, true);
