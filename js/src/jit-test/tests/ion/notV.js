function foo(x) {
   return !x;
}

assertEq(foo({}), false);
assertEq(foo({}), false);
assertEq(foo(1.1), false);
assertEq(foo(1.1), false);
assertEq(foo(0.0), true);
assertEq(foo(0.0), true);
assertEq(foo(null), true);
assertEq(foo(null), true);
assertEq(foo(undefined), true);
assertEq(foo(undefined), true);
assertEq(foo(Infinity), false);
assertEq(foo(Infinity), false);
assertEq(foo(NaN), true);
assertEq(foo(NaN), true);
assertEq(foo([]), false);
assertEq(foo([]), false);
assertEq(foo(''), true);
assertEq(foo(''), true);
assertEq(foo('x'), false);
assertEq(foo('x'), false);
assertEq(foo(true), false);
assertEq(foo(true), false);
assertEq(foo(false), true);
assertEq(foo(false), true);
assertEq(foo(-0.0), true);
assertEq(foo(-0.0), true);
assertEq(foo(objectEmulatingUndefined()), true);
assertEq(foo(objectEmulatingUndefined()), true);
