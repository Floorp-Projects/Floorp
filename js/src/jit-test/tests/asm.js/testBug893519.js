// |jit-test| error:Error; skip-if: !isAsmJSCompilationAvailable()

var g = newGlobal({newCompartment: true});
evaluate("function h() { function f() { 'use asm'; function g() { return 42 } return g } return f }", { global:g});
var h = clone(g.h);
assertEq(h()()(), 42);
