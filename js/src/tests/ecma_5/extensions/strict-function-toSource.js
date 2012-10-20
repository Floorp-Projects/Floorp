/*
 * Bug 800407 - Functions defined with Function construcor
 * do have strict mode when JSOPTION_STRICT_MODE is on.
 */

options("strict_mode");
function testRunOptionStrictMode(str, arg, result) {
    var strict_inner = new Function('return typeof this == "undefined";');
    return strict_inner;
}
assertEq(eval(uneval(testRunOptionStrictMode()))(), true);

if (typeof decompileBody !== "undefined") {
    assertEq(decompileBody(new Function('x', 'return x*2;')).contains('\n"use strict"'), true);
}

reportCompare(true, true);
