var g = newGlobal();
evaluate("function h() { function f() { 'use asm'; function g() { return 42 } return g } return f }", { compileAndGo:false, global:g});
var h = clone(g.h);
assertEq(h()()(), 42);
