// uncaughtExceptionHook returns a resumption value.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = new Debugger(g);
var rv;
dbg.onDebuggerStatement = function () { throw 15; };
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
assertThrowsValue(function () { g.eval("debugger"); }, 57);

// case 3: return
rv = {return: 42};
assertEq(g.eval("debugger;"), 42);
