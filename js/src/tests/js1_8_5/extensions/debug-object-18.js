// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Uncaught exceptions in the debugger itself are delivered to the
// uncaughtExceptionHook.

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var log;
dbg.hooks = {
    debuggerHandler: function () {
        log += 'x';
        throw new TypeError("fail");
    }
};
dbg.uncaughtExceptionHook = function (exc) {
    assertEq(exc instanceof TypeError, true);
    log += '!';
};

log = '';
g.eval("debugger");
assertEq(log, 'x!');

reportCompare(0, 0, 'ok');
