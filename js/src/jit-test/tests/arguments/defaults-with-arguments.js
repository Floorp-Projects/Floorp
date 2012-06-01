function f(a=1, b=2, c=3) { return arguments; }
var args = f();
assertEq(args.length, 0);
assertEq("0" in args, false);
args = f(5, 6);
assertEq(args.length, 2);
assertEq(args[1], 6);
args = f(9, 8, 7, 6, 5);
assertEq(args.length, 5);
assertEq(args[4], 5);
