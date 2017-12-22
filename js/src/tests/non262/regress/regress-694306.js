// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var x;
try {
    eval("var {if} = {'if': 1};");
} catch (exc) {
    x = exc;
}
assertEq(x instanceof SyntaxError, true);
assertEq("if" in this, false);

x = undefined;
try {
    Function("var {if} = {'if': 1};");
} catch (exc) {
    x = exc;
}
assertEq(x instanceof SyntaxError, true);

reportCompare(0, 0, 'ok');
