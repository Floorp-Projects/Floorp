// |jit-test| debug
// frame.eval SyntaxErrors are reflected, not thrown

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var exc, SEp;
dbg.hooks = {
    debuggerHandler: function (frame) {
        exc = frame.eval("#$@!").throw;
        SEp = frame.eval("SyntaxError.prototype").return;
    }
};
g.eval("debugger;");
assertEq(exc.getPrototype(), SEp);
