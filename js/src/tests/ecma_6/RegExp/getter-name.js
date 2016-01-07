var BUGNUMBER = 1180290;
var summary = 'RegExp getters should have get prefix';

print(BUGNUMBER + ": " + summary);

// FIXME: bug 887016
// assertEq(Object.getOwnPropertyDescriptor(RegExp, Symbol.species).get.name, "get [Symbol.species]");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get.name, "get flags");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "global").get.name, "get global");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "ignoreCase").get.name, "get ignoreCase");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "multiline").get.name, "get multiline");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "source").get.name, "get source");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "sticky").get.name, "get sticky");
assertEq(Object.getOwnPropertyDescriptor(RegExp.prototype, "unicode").get.name, "get unicode");

if (typeof reportCompare === 'function')
  reportCompare(true, true);
