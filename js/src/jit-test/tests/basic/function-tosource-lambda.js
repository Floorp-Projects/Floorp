var f1 = function f0(a, b) { return a + b; }
assertEq(f1.toSource(), "(function f0(a, b) { return a + b; })");
assertEq(f1.toString(), "function f0(a, b) { return a + b; }");
assertEq(decompileFunction(f1), f1.toString());
assertEq(decompileBody(f1), " return a + b; ");
var f2 = function (a, b) { return a + b; };
assertEq(f2.toSource(), "(function (a, b) { return a + b; })");
assertEq(f2.toString(), "function (a, b) { return a + b; }");
assertEq(decompileFunction(f2), f2.toString());
assertEq(decompileBody(f2), " return a + b; ");
var f3 = (function (a, b) { return a + b; });
assertEq(f3.toSource(), "(function (a, b) { return a + b; })");
assertEq(f3.toString(), "function (a, b) { return a + b; }");
assertEq(decompileFunction(f3), f3.toString());
assertEq(decompileBody(f3), " return a + b; ");
