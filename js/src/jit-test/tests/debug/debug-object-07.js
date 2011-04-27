// |jit-test| debug
// Simple {return:} resumption.

var g = newGlobal('new-compartment');

var dbg = Debug(g);
dbg.hooks = {
    debuggerHandler: function (stack) {
        return {return: 1234};
    }
};

assertEq(g.eval("debugger; false;"), 1234);
g.eval("function f() { debugger; return 'bad'; }");
assertEq(g.f(), 1234);
