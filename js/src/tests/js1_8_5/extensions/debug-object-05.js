// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// A debugger statement in a debuggerHandler should not reenter.

var g = newGlobal('new-compartment');
var calls = 0;

var dbg = Debug(g);
dbg.hooks = {
    debuggerHandler: function (stack) {
        calls++;
        debugger;
    }
};

assertEq(g.eval("debugger; 7;"), 7);
assertEq(calls, 1);

reportCompare(0, 0, 'ok');
