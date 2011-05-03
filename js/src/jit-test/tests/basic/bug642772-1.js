var n1 = Number.prototype.toFixed;
var s1 = String.prototype.split;
delete Number;
delete String;

var n2 = (5).toFixed;
var s2 = ("foo").split;

// Check enumeration doesn't resurrect deleted standard classes
for (x in this) {}

// Ensure the prototypes are shared.
var n3 = (5).toFixed;
var s3 = ("foo").split;

assertEq(s1, s2);
assertEq(s1, s3);
assertEq(n1, n2);
assertEq(n1, n3);
