// Test that bytecode is still shared when expected. The expectations can be
// updated if necessary.


// Check the testing function.
evaluate(`function a01(){ return a + b; }`)
evaluate(`function a02(){ return a + b; }`)
evaluate(`function a03(){ return a + b + c; }`)
assertEq(hasSameBytecodeData(a01, a02), true)
assertEq(hasSameBytecodeData(a01, a03), false)


// Check effect of whitespace.
evaluate(`function   b(){}`)
evaluate(`function b01(){}`)
evaluate(`function b02 (){}`)
evaluate(`function b03( ){}`)
evaluate(`function b04() {}`)
evaluate(`function b05(){ }`)
assertEq(hasSameBytecodeData(b, b01), true)
assertEq(hasSameBytecodeData(b, b02), true)
assertEq(hasSameBytecodeData(b, b03), false)
assertEq(hasSameBytecodeData(b, b04), false)
assertEq(hasSameBytecodeData(b, b05), false)


// Check effect of binding names.
evaluate(`function c01(){ return a; }`)
evaluate(`function c02(){ return b; }`)
evaluate(`function c03(){ return cc; }`)
assertEq(hasSameBytecodeData(c01, c02), true)
assertEq(hasSameBytecodeData(c01, c03), false)


// Check effect of string literals.
evaluate(`function d01(){ return "AA"; }`)
evaluate(`function d02(){ return "BB"; }`)
assertEq(hasSameBytecodeData(d01, d02), true)


// Check effect of string template literals.
evaluate("function e01(){ return a`AA`; }")
evaluate("function e02(){ return b`BB`; }")
assertEq(hasSameBytecodeData(e01, e02), true)


// Check effect of object literals.
evaluate(`function f01(){ return { a: 1 }; }`)
evaluate(`function f02(){ return { b: 1 }; }`)
evaluate(`function f03(){ return { b: 2 }; }`)
assertEq(hasSameBytecodeData(f01, f02), true)
assertEq(hasSameBytecodeData(f01, f03), false)


// Check effect of inner functions.
evaluate(`function g01(){ return () => 0; }`)
evaluate(`function g02(){ return () => 0; }`)
evaluate(`function g03(){ return () => a; }`)
assertEq(hasSameBytecodeData(g01, g02), true)
assertEq(hasSameBytecodeData(g01, g03), true)


// Check effect of line number.
evaluate(`function h01(){ return 0; }`)
evaluate(`\nfunction h02(){ return 0; }`)
evaluate(`\n\n\n\n\n\n\nfunction h03(){ return 0; }`)
assertEq(hasSameBytecodeData(h01, h02), true)
assertEq(hasSameBytecodeData(h01, h03), true)


// Check effect of line number when function has large gaps in it. This
// corresponds to SetLine source-notes.
evaluate(`function i01(){ return\n\n\n\n\n\n\n\n0; }`)
evaluate(`\nfunction i02(){ return\n\n\n\n\n\n\n\n0; }`)
evaluate(`\n\n\n\n\n\n\nfunction i03(){ return\n\n\n\n\n\n\n\n0; }`)
assertEq(hasSameBytecodeData(i01, i02), true)
assertEq(hasSameBytecodeData(i01, i03), true)


// Check effect of column number.
evaluate(`function j01(){ return 0; }`)
evaluate(` function j02(){ return 0; }`)
evaluate(` \tfunction j03(){ return 0; }`)
assertEq(hasSameBytecodeData(j01, j02), true)
assertEq(hasSameBytecodeData(j01, j03), true)


// Check different forms of functions.
evaluate(`function k01      ()    { return 0; }`)
evaluate(`var k02 = function()    { return 0; }`)
evaluate(`var k03 =         () => { return 0; }`)
assertEq(hasSameBytecodeData(k01, k02), true)
assertEq(hasSameBytecodeData(k01, k03), true)


// Check sharing across globals / compartments.
let l02_global = newGlobal({newCompartment: false});
let l03_global = newGlobal({newCompartment: true});
evaluate(`function l01() { return 0; }`)
l02_global.evaluate(`function l02() { return 0; }`)
l03_global.evaluate(`function l03() { return 0; }`)
assertEq(hasSameBytecodeData(l01, l02_global.l02), true)
assertEq(hasSameBytecodeData(l01, l03_global.l03), true)
