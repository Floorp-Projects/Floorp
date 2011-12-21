function f1() { let (arguments = 42) { return arguments } }
assertEq(f1(), 42);

function f2() { let (arguments) { return arguments } }
assertEq(f2(), undefined);
