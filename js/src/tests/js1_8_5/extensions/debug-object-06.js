// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Simple {throw:} resumption.

var g = newGlobal('new-compartment');

var dbg = Debug(g);
dbg.hooks = {
    debuggerHandler: function (stack) {
        return {throw: "oops"};
    }
};

var result = 'no exception thrown';
try {
    g.eval("debugger;");
} catch (exc) {
    result = exc;
}
assertEq(result, "oops");

g.eval("function f() { debugger; }");
result = 'no exception thrown';
try {
    g.f();
} catch (exc) {
    result = exc;
}
assertEq(result, "oops");

reportCompare(0, 0, 'ok');
