var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- Pattern match should start from lead surrogate when lastIndex points corresponding trail surrogate.";

print(BUGNUMBER + ": " + summary);

var r = /\uD83D\uDC38/ug;
r.lastIndex = 1;
var str = "\uD83D\uDC38";
var result = r.exec(str);
assertEq(result.length, 1);
assertEq(result[0], "\uD83D\uDC38");

// This does not match to ES6 spec, but the spec will be changed.
assertEq(result.index, 0);

if (typeof reportCompare === "function")
    reportCompare(true, true);
