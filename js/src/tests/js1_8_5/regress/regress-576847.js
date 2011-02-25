/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Don't crash. */
try {
    eval("function f(){}(((f)for(x in function(){}))())");
    var threwTypeError = false;
} catch (x) {
    var threwTypeError = x instanceof TypeError;
}
assertEq(threwTypeError, true);

/* Properly bind f. */
assertEq(eval("function f() {}; var i = (f for (f in [1])); uneval([n for (n in i)])"),
         '["0"]');

reportCompare(true, true);
