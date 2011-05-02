// |jit-test| debug
// Simple {throw:} resumption.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');

var dbg = Debug(g);
dbg.hooks = {
    debuggerHandler: function (stack) {
        return {throw: "oops"};
    }
};

assertThrowsValue(function () { g.eval("debugger;"); }, "oops");

g.eval("function f() { debugger; }");
assertThrowsValue(function () { g.f(); }, "oops");
