// 'arguments' is lexically scoped in genexprs at toplevel.

var arguments = 8;
assertEq((arguments for (x of [1])).next(), 8);
