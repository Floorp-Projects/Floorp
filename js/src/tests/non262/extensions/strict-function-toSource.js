/*
 * Bug 800407 - Functions defined with Function construcor
 * do have strict mode when JSOPTION_STRICT_MODE is on.
 */

options("strict_mode");
function testRunOptionStrictMode(str, arg, result) {
    var strict_inner = new Function('return typeof this == "undefined";');
    return strict_inner;
}
let inner = testRunOptionStrictMode();
assertEq(inner(), true);
assertEq(eval(uneval(inner))(), true);

assertEq((new Function('x', 'return x*2;')).toSource().includes('"use strict"'), false);

reportCompare(true, true);
