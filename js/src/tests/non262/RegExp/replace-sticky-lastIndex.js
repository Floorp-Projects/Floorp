var BUGNUMBER = 887016;
var summary = "String.prototype.replace should do nothing if lastIndex is invalid for sticky RegExp";

print(BUGNUMBER + ": " + summary);

var re = /a/y;
re.lastIndex = -1;
assertEq("a".replace(re, "b"), "b");
re.lastIndex = 0;
assertEq("a".replace(re, "b"), "b");
re.lastIndex = 1;
assertEq("a".replace(re, "b"), "a");
re.lastIndex = 2;
assertEq("a".replace(re, "b"), "a");
re.lastIndex = "foo";
assertEq("a".replace(re, "b"), "b");
re.lastIndex = "1";
assertEq("a".replace(re, "b"), "a");
re.lastIndex = {};
assertEq("a".replace(re, "b"), "b");

if (typeof reportCompare === "function")
    reportCompare(true, true);
