// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// uncaughtExceptionHook returns a resumption value.

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var rv;
dbg.hooks = {debuggerHandler: function () { throw 15; }};
dbg.uncaughtExceptionHook = function (exc) {
    assertEq(exc, 15);
    return rv;
};

// case 1: undefined
rv = undefined;
g.eval("debugger");

// case 2: throw
rv = {throw: 57};
var result;
try {
    g.eval("debugger");
    result = 'no exception thrown';
} catch (exc) {
    result = 'caught ' + exc;
}
assertEq(result, 'caught 57');

// case 3: return
rv = {return: 42};
assertEq(g.eval("debugger;"), 42);

reportCompare(0, 0, 'ok');
