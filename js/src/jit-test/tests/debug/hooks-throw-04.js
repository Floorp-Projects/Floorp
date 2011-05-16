// |jit-test| debug
// hooks.throw is not called for exceptions thrown and handled in the debugger.

var g = newGlobal('new-compartment');
var dbg = Debug(g);
g.log = '';
dbg.hooks = {
    debuggerHandler: function (frame) {
        try {
            throw new Error("oops");
        } catch (exc) {
            g.log += exc.message;
        }
    },
    throw: function (frame) {
        g.log += 'BAD';
    }
};

g.eval("debugger; log += ' ok';");
assertEq(g.log, 'oops ok');
