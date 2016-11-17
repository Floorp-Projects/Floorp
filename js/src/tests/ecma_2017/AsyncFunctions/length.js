var BUGNUMBER = 1185106;
var summary = "async function length";

print(BUGNUMBER + ": " + summary);

assertEq(async function() {}.length, 0);
assertEq(async function(a) {}.length, 1);
assertEq(async function(a, b, c) {}.length, 3);
assertEq(async function(a, b, c, ...d) {}.length, 3);

if (typeof reportCompare === "function")
    reportCompare(true, true);
