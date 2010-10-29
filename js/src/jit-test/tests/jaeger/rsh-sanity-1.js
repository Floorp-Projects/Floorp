/* Unknown types. */
function rsh(lhs, rhs) { return lhs >> rhs; }
assertEq(rsh(1024, 2), 256)
assertEq(rsh(1024.5, 2), 256)
assertEq(rsh(1024.5, 2.0), 256)

/* Constant rhs. */
var lhs = 1024;
assertEq(lhs >> 2, 256);
lhs = 1024.5;
assertEq(lhs >> 2, 256);

/* Constant lhs. */
var rhs = 2;
assertEq(256, 1024 >> rhs);
var rhs = 2.0;
assertEq(256, 1024 >> rhs);
