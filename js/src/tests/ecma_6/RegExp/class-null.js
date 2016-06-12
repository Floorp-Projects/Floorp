var BUGNUMBER = 1279467;
var summary = "Null in character class in RegExp with unicode flag.";

print(BUGNUMBER + ": " + summary);

var m = /([\0]+)/u.exec("\u0000");
assertEq(m.length, 2);
assertEq(m[0], '\u0000');
assertEq(m[1], '\u0000');

var m = /([\0]+)/u.exec("0");
assertEq(m, null);

if (typeof reportCompare === "function")
    reportCompare(true, true);
